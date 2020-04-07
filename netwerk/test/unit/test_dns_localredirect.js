"use strict";

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
  Ci.nsIPrefBranch
);

var nextTest;

var listener = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");

    nextTest();
    do_test_finished();
  },
  QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
};

const defaultOriginAttributes = {};

function run_test() {
  prefs.setCharPref("network.dns.localDomains", "local.vingtetun.org");

  var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
    Ci.nsIThreadManager
  );
  var mainThread = threadManager.currentThread;
  nextTest = do_test_2;
  dns.asyncResolve(
    "local.vingtetun.org",
    0,
    listener,
    mainThread,
    defaultOriginAttributes
  );

  do_test_pending();
}

function do_test_2() {
  var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
    Ci.nsIThreadManager
  );
  var mainThread = threadManager.currentThread;
  nextTest = testsDone;
  prefs.setCharPref("network.dns.forceResolve", "localhost");
  dns.asyncResolve(
    "www.example.com",
    0,
    listener,
    mainThread,
    defaultOriginAttributes
  );

  do_test_pending();
}

function testsDone() {
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.dns.forceResolve");
}
