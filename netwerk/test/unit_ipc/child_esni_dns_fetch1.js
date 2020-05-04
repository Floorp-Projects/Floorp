"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
  Ci.nsIThreadManager
);
const mainThread = threadManager.currentThread;

const defaultOriginAttributes = {};

let test_answer = "bXkgdm9pY2UgaXMgbXkgcGFzc3dvcmQ=";
let test_answer_addr = "127.0.0.1";

class DNSListener {
  constructor() {
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
  }
  onLookupComplete(inRequest, inRecord, inStatus) {
    this.resolve([inRequest, inRecord, inStatus]);
  }
  // So we can await this as a promise.
  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}

DNSListener.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIDNSListener,
]);

add_task(async function testEsniRequest() {
  // use the h2 server as DOH provider
  let listenerEsni = new DNSListener();
  let request = dns.asyncResolveByType(
    "_esni.example.com",
    dns.RESOLVE_TYPE_TXT,
    0,
    listenerEsni,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await listenerEsni;
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  Assert.equal(inRequest, request, "correct request was used");
  let answer = inRecord
    .QueryInterface(Ci.nsIDNSTXTRecord)
    .getRecordsAsOneString();
  Assert.equal(answer, test_answer, "got correct answer");
});

add_task(async function testEsniHTTPSSVC() {
  // use the h2 server as DOH provider
  let listenerEsni = new DNSListener();
  let request = dns.asyncResolveByType(
    "httpssvc_esni.example.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    listenerEsni,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await listenerEsni;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  let esni = answer[0].values[0].QueryInterface(Ci.nsISVCParamEsniConfig);
  Assert.equal(esni.esniConfig, "testytestystringstring", "got correct answer");
});
