"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const defaultOriginAttributes = {};
const mainThread = Services.tm.currentThread;

const overrideService = Cc[
  "@mozilla.org/network/native-dns-override;1"
].getService(Ci.nsINativeDNSResolverOverride);

class Listener {
  constructor() {
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
  }

  onLookupComplete(inRequest, inRecord, inStatus) {
    this.resolve([inRequest, inRecord, inStatus]);
  }

  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}

Listener.prototype.QueryInterface = ChromeUtils.generateQI(["nsIDNSListener"]);

const DOMAIN_IDN = "bücher.org";
const ACE_IDN = "xn--bcher-kva.org";

const ADDR1 = "127.0.0.1";
const ADDR2 = "::1";

add_task(async function test_dns_localhost() {
  let listener = new Listener();
  dns.asyncResolve(
    "localhost",
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );
  let [, inRecord] = await listener;
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  let answer = inRecord.getNextAddrAsString();
  Assert.ok(answer == ADDR1 || answer == ADDR2);
});

add_task(async function test_idn_cname() {
  let listener = new Listener();
  dns.asyncResolve(
    DOMAIN_IDN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );
  let [, inRecord] = await listener;
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  Assert.equal(inRecord.canonicalName, ACE_IDN, "IDN is returned as punycode");
});

add_task(
  {
    skip_if: () =>
      Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT,
  },
  async function test_long_domain() {
    let listener = new Listener();
    let domain = "a".repeat(253);
    overrideService.addIPOverride(domain, "1.2.3.4");
    dns.asyncResolve(
      domain,
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
      null, // resolverInfo
      listener,
      mainThread,
      defaultOriginAttributes
    );
    let [, , inStatus] = await listener;
    Assert.equal(inStatus, Cr.NS_OK);

    listener = new Listener();
    domain = "a".repeat(254);
    overrideService.addIPOverride(domain, "1.2.3.4");

    Services.prefs.setBoolPref("network.dns.limit_253_chars", true);

    if (mozinfo.socketprocess_networking) {
      // When using the socket process, the call fails asynchronously.
      dns.asyncResolve(
        domain,
        Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
        Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
        null, // resolverInfo
        listener,
        mainThread,
        defaultOriginAttributes
      );
      let [, , inStatus] = await listener;
      Assert.equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);
    } else {
      Assert.throws(
        () => {
          dns.asyncResolve(
            domain,
            Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
            Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
            null, // resolverInfo
            listener,
            mainThread,
            defaultOriginAttributes
          );
        },
        /NS_ERROR_UNKNOWN_HOST/,
        "Should throw for large domains"
      );
    }

    listener = new Listener();
    domain = "a".repeat(254);
    Services.prefs.setBoolPref("network.dns.limit_253_chars", false);
    dns.asyncResolve(
      domain,
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
      null, // resolverInfo
      listener,
      mainThread,
      defaultOriginAttributes
    );
    [, , inStatus] = await listener;
    Assert.equal(inStatus, Cr.NS_OK);

    Services.prefs.clearUserPref("network.dns.limit_253_chars");
    overrideService.clearOverrides();
  }
);
