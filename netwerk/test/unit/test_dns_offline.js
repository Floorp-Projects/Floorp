var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var ioService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
var mainThread = threadManager.currentThread;

var listener1 = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_ERROR_OFFLINE);
    test2();
    do_test_finished();
  }
};

var listener2 = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_OK);
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");
    test3();
    do_test_finished();
  }
};

var listener3 = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    Assert.equal(inStatus, Cr.NS_OK);
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");
    cleanup();
    do_test_finished();
  }
};

const defaultOriginAttributes = {};

function run_test() {
  do_test_pending();
  prefs.setBoolPref("network.dns.offline-localhost", false);
  ioService.offline = true;
  try {
    dns.asyncResolve("localhost", 0, listener1, mainThread, defaultOriginAttributes);
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
  dns.asyncResolve("localhost", 0, listener2, mainThread, defaultOriginAttributes);
}

function test3() {
  do_test_pending();
  ioService.offline = false;
  // we need to let the main thread run and apply the changes
  do_timeout(0, test3Continued);
}

function test3Continued() {
  dns.asyncResolve("localhost", 0, listener3, mainThread, defaultOriginAttributes);
}

function cleanup() {
  prefs.clearUserPref("network.dns.offline-localhost");
}
