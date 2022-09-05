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

  async firstAddress() {
    let all = await this.addresses();
    if (all.length) {
      return all[0];
    }

    return undefined;
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

const DOMAIN = "example.org";
const OTHER = "example.com";

add_task(async function test_bad_IPs() {
  Assert.throws(
    () => override.addIPOverride(DOMAIN, DOMAIN),
    /NS_ERROR_UNEXPECTED/,
    "Should throw if input is not an IP address"
  );
  Assert.throws(
    () => override.addIPOverride(DOMAIN, ""),
    /NS_ERROR_UNEXPECTED/,
    "Should throw if input is not an IP address"
  );
  Assert.throws(
    () => override.addIPOverride(DOMAIN, " "),
    /NS_ERROR_UNEXPECTED/,
    "Should throw if input is not an IP address"
  );
  Assert.throws(
    () => override.addIPOverride(DOMAIN, "1-2-3-4"),
    /NS_ERROR_UNEXPECTED/,
    "Should throw if input is not an IP address"
  );
});

add_task(async function test_ipv4() {
  let listener = new Listener();
  override.addIPOverride(DOMAIN, "1.2.3.4");
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  Assert.equal(await listener.firstAddress(), "1.2.3.4");

  dns.clearCache(false);
  override.clearOverrides();
});

add_task(async function test_ipv6() {
  let listener = new Listener();
  override.addIPOverride(DOMAIN, "fe80::6a99:9b2b:6ccc:6e1b");
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  Assert.equal(await listener.firstAddress(), "fe80::6a99:9b2b:6ccc:6e1b");

  dns.clearCache(false);
  override.clearOverrides();
});

add_task(async function test_clearOverrides() {
  let listener = new Listener();
  override.addIPOverride(DOMAIN, "1.2.3.4");
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  Assert.equal(await listener.firstAddress(), "1.2.3.4");

  dns.clearCache(false);
  override.clearOverrides();

  listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  Assert.notEqual(await listener.firstAddress(), "1.2.3.4");

  await new Promise(resolve => do_timeout(1000, resolve));
  dns.clearCache(false);
  override.clearOverrides();
});

add_task(async function test_clearHostOverride() {
  override.addIPOverride(DOMAIN, "2.2.2.2");
  override.addIPOverride(OTHER, "2.2.2.2");
  override.clearHostOverride(DOMAIN);
  let listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );

  Assert.notEqual(await listener.firstAddress(), "2.2.2.2");

  listener = new Listener();
  dns.asyncResolve(
    OTHER,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  Assert.equal(await listener.firstAddress(), "2.2.2.2");

  // Note: this test will use the actual system resolver. On windows we do a
  // second async call to the system libraries to get the TTL values, which
  // keeps the record alive after the onLookupComplete()
  // We need to wait for a bit, until the second call is finished before we
  // can clear the cache to make sure we evict everything.
  // If the next task ever starts failing, with an IP that is not in this
  // file, then likely the timeout is too small.
  await new Promise(resolve => do_timeout(1000, resolve));
  dns.clearCache(false);
  override.clearOverrides();
});

add_task(async function test_multiple_IPs() {
  override.addIPOverride(DOMAIN, "2.2.2.2");
  override.addIPOverride(DOMAIN, "1.1.1.1");
  override.addIPOverride(DOMAIN, "::1");
  override.addIPOverride(DOMAIN, "fe80::6a99:9b2b:6ccc:6e1b");
  let listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  Assert.deepEqual(await listener.addresses(), [
    "2.2.2.2",
    "1.1.1.1",
    "::1",
    "fe80::6a99:9b2b:6ccc:6e1b",
  ]);

  dns.clearCache(false);
  override.clearOverrides();
});

add_task(async function test_address_family_flags() {
  override.addIPOverride(DOMAIN, "2.2.2.2");
  override.addIPOverride(DOMAIN, "1.1.1.1");
  override.addIPOverride(DOMAIN, "::1");
  override.addIPOverride(DOMAIN, "fe80::6a99:9b2b:6ccc:6e1b");
  let listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  Assert.deepEqual(await listener.addresses(), [
    "::1",
    "fe80::6a99:9b2b:6ccc:6e1b",
  ]);

  listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    Ci.nsIDNSService.RESOLVE_DISABLE_IPV6,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  Assert.deepEqual(await listener.addresses(), ["2.2.2.2", "1.1.1.1"]);

  dns.clearCache(false);
  override.clearOverrides();
});

add_task(async function test_cname_flag() {
  override.addIPOverride(DOMAIN, "2.2.2.2");
  let listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  let [, inRecord] = await listener;
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  Assert.throws(
    () => inRecord.canonicalName,
    /NS_ERROR_NOT_AVAILABLE/,
    "No canonical name flag"
  );
  Assert.equal(inRecord.getNextAddrAsString(), "2.2.2.2");

  listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  [, inRecord] = await listener;
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  Assert.equal(inRecord.canonicalName, DOMAIN, "No canonical name specified");
  Assert.equal(inRecord.getNextAddrAsString(), "2.2.2.2");

  dns.clearCache(false);
  override.clearOverrides();

  override.addIPOverride(DOMAIN, "2.2.2.2");
  override.setCnameOverride(DOMAIN, OTHER);
  listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  [, inRecord] = await listener;
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  Assert.equal(inRecord.canonicalName, OTHER, "Must have correct CNAME");
  Assert.equal(inRecord.getNextAddrAsString(), "2.2.2.2");

  dns.clearCache(false);
  override.clearOverrides();
});

add_task(async function test_nxdomain() {
  override.addIPOverride(DOMAIN, "N/A");
  let listener = new Listener();
  dns.asyncResolve(
    DOMAIN,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
    null,
    listener,
    mainThread,
    defaultOriginAttributes
  );

  let [, , inStatus] = await listener;
  equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);
});
