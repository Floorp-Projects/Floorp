"use strict";

var ioService = Services.io;
var prefs = Services.prefs;
var mainThread = Services.tm.currentThread;

var listener1 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_ERROR_OFFLINE);
    test2();
    do_test_finished();
  },
};

var listener2 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_OK);
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");
    test3();
    do_test_finished();
  },
};

var listener3 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_OK);
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");
    cleanup();
    do_test_finished();
  },
};

const defaultOriginAttributes = {};

function run_test() {
  do_test_pending();
  prefs.setBoolPref("network.dns.offline-localhost", false);
  // We always resolve localhost as it's hardcoded without the following pref:
  prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);
  ioService.offline = true;
  try {
    Services.dns.asyncResolve(
      "localhost",
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      0,
      null, // resolverInfo
      listener1,
      mainThread,
      defaultOriginAttributes
    );
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_OFFLINE);
    test2();
    do_test_finished();
  }
}

function test2() {
  do_test_pending();
  prefs.setBoolPref("network.dns.offline-localhost", true);
  ioService.offline = false;
  ioService.offline = true;
  // we need to let the main thread run and apply the changes
  do_timeout(0, test2Continued);
}

function test2Continued() {
  Services.dns.asyncResolve(
    "localhost",
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    listener2,
    mainThread,
    defaultOriginAttributes
  );
}

function test3() {
  do_test_pending();
  ioService.offline = false;
  // we need to let the main thread run and apply the changes
  do_timeout(0, test3Continued);
}

function test3Continued() {
  Services.dns.asyncResolve(
    "localhost",
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    listener3,
    mainThread,
    defaultOriginAttributes
  );
}

function cleanup() {
  prefs.clearUserPref("network.dns.offline-localhost");
  prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
}
