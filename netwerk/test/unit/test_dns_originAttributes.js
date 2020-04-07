"use strict";

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
  Ci.nsIThreadManager
);
var mainThread = threadManager.currentThread;

var listener1 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_OK);
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");
    test2();
    do_test_finished();
  },
};

var listener2 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_OK);
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");
    test3();
    do_test_finished();
  },
};

var listener3 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_ERROR_OFFLINE);
    do_test_finished();
  },
};

const firstOriginAttributes = { userContextId: 1 };
const secondOriginAttributes = { userContextId: 2 };

// First, we resolve the address normally for first originAttributes.
function run_test() {
  do_test_pending();
  dns.asyncResolve(
    "localhost",
    0,
    listener1,
    mainThread,
    firstOriginAttributes
  );
}

// Second, we resolve the same address offline to see whether its DNS cache works
// correctly.
function test2() {
  do_test_pending();
  dns.asyncResolve(
    "localhost",
    Ci.nsIDNSService.RESOLVE_OFFLINE,
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
    dns.asyncResolve(
      "localhost",
      Ci.nsIDNSService.RESOLVE_OFFLINE,
      listener3,
      mainThread,
      secondOriginAttributes
    );
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_OFFLINE);
    do_test_finished();
  }
}
