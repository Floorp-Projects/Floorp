"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const defaultOriginAttributes = {};
const threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
  Ci.nsIThreadManager
);
const mainThread = threadManager.currentThread;

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

const DOMAIN = "localhost";
const ADDR1 = "127.0.0.1";
const ADDR2 = "::1";

add_task(async function test_dns_localhost() {
  let listener = new Listener();
  dns.asyncResolve(
    "localhost",
    0,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  let [inRequest, inRecord, inStatus] = await listener;

  let answer = inRecord.getNextAddrAsString();
  Assert.ok(answer == ADDR1 || answer == ADDR2);
});

add_task(async function test_idn_cname() {
  let listener = new Listener();
  dns.asyncResolve(
    DOMAIN_IDN,
    Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
    listener,
    mainThread,
    defaultOriginAttributes
  );
  let [inRequest, inRecord, inStatus] = await listener;
  Assert.equal(inRecord.canonicalName, ACE_IDN, "IDN is returned as punycode");
});
