"use strict";

var prefs = Services.prefs;
var mainThread = Services.tm.currentThread;

var listener1 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_OK);
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");
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
    Assert.equal(inStatus, Cr.NS_ERROR_OFFLINE);
    cleanup();
    do_test_finished();
  },
};

const firstOriginAttributes = { userContextId: 1 };
const secondOriginAttributes = { userContextId: 2 };

// First, we resolve the address normally for first originAttributes.
function run_test() {
  do_test_pending();
  prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);
  Services.dns.asyncResolve(
    "localhost",
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    listener1,
    mainThread,
    firstOriginAttributes
  );
}

// Second, we resolve the same address offline to see whether its DNS cache works
// correctly.
function test2() {
  do_test_pending();
  Services.dns.asyncResolve(
    "localhost",
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    Ci.nsIDNSService.RESOLVE_OFFLINE,
    null, // resolverInfo
    listener2,
    mainThread,
    firstOriginAttributes
  );
}

// Third, we resolve the same address offline again with different originAttributes.
// This resolving should fail since the DNS cache of the given address is not exist
// for this originAttributes.
function test3() {
  do_test_pending();
  try {
    Services.dns.asyncResolve(
      "localhost",
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      Ci.nsIDNSService.RESOLVE_OFFLINE,
      null, // resolverInfo
      listener3,
      mainThread,
      secondOriginAttributes
    );
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_OFFLINE);
    cleanup();
    do_test_finished();
  }
}

function cleanup() {
  prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
}
