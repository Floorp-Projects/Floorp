"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);
const defaultOriginAttributes = {};
const mainThread = Services.tm.currentThread;

class Listener {
  constructor() {
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
  }

  onLookupComplete(inRequest, inRecord, inStatus) {
    this.resolve([inRequest, inRecord, inStatus]);
  }

  async addresses() {
    let [, inRecord] = await this.promise;
    let addresses = [];
    if (!inRecord) {
      return addresses; // returns []
    }
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    while (inRecord.hasMore()) {
      addresses.push(inRecord.getNextAddrAsString());
    }
    return addresses;
  }

  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}
Listener.prototype.QueryInterface = ChromeUtils.generateQI(["nsIDNSListener"]);

const DOMAINS = [
  "localhost",
  "localhost.",
  "vhost.localhost",
  "vhost.localhost.",
];
DOMAINS.forEach(domain => {
  add_task(async function test_() {
    let listener1 = new Listener();
    const overrides = ["1.2.3.4", "5.6.7.8"];
    overrides.forEach(ip_address => {
      override.addIPOverride(domain, ip_address);
    });

    // Verify that loopback host names are not overridden.
    dns.asyncResolve(
      domain,
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      0,
      null,
      listener1,
      mainThread,
      defaultOriginAttributes
    );
    Assert.deepEqual(
      await listener1.addresses(),
      ["127.0.0.1", "::1"],
      `${domain} is not overridden`
    );

    // Verify that if localhost hijacking is enabled, the overrides
    // registered above are taken into account.
    Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);
    let listener2 = new Listener();
    dns.asyncResolve(
      domain,
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      0,
      null,
      listener2,
      mainThread,
      defaultOriginAttributes
    );
    Assert.deepEqual(
      await listener2.addresses(),
      overrides,
      `${domain} is overridden`
    );
    Services.prefs.clearUserPref("network.proxy.allow_hijacking_localhost");

    dns.clearCache(false);
    override.clearOverrides();
  });
});
