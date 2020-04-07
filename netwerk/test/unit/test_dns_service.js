"use strict";

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);

var listener = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");

    do_test_finished();
  },
  QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
};

const defaultOriginAttributes = {};

function run_test() {
  var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
    Ci.nsIThreadManager
  );
  var mainThread = threadManager.currentThread;
  dns.asyncResolve(
    "localhost",
    0,
    listener,
    mainThread,
    defaultOriginAttributes
  );

  do_test_pending();
}
