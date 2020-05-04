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
    let txtRec;
    try {
      txtRec = inRecord.QueryInterface(Ci.nsIDNSByTypeRecord);
    } catch (e) {}
    if (txtRec) {
      this.resolve([inRequest, txtRec, inStatus, "onLookupByTypeComplete"]);
    } else {
      this.resolve([inRequest, inRecord, inStatus, "onLookupComplete"]);
    }
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

  let [inRequest, inRecord, inStatus, inType] = await listenerEsni;
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inType, "onLookupByTypeComplete", "check correct type");
  let answer = inRecord.getRecordsAsOneString();
  Assert.equal(answer, test_answer, "got correct answer");
});
