"use strict";

var prefs = Services.prefs;

var nextTest;

var listener = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
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

  var mainThread = Services.tm.currentThread;
  nextTest = do_test_2;
  Services.dns.asyncResolve(
    "local.vingtetun.org",
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  do_test_pending();
}

function do_test_2() {
  var mainThread = Services.tm.currentThread;
  nextTest = testsDone;
  prefs.setCharPref("network.dns.forceResolve", "localhost");
  Services.dns.asyncResolve(
    "www.example.com",
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
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
