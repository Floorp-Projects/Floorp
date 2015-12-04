var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
var mainThread = threadManager.currentThread;

var onionPref;
var localdomainPref;
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

var listener1 = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    var answer = inRecord.getNextAddrAsString();
    do_check_true(answer == "127.0.0.1" || answer == "::1");
    do_test_2();
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var listener2 = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    do_check_false(Components.isSuccessCode(inStatus));
    do_test_3();
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var listener3 = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    var answer = inRecord.getNextAddrAsString();
    do_check_true(answer == "127.0.0.1" || answer == "::1");
    all_done();
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function do_test_3() {
  prefs.setBoolPref("network.dns.blockDotOnion", false);
  dns.asyncResolve("private.onion", 0, listener3, mainThread);
}

function do_test_2() {
  prefs.setBoolPref("network.dns.blockDotOnion", true);
  try {
    dns.asyncResolve("private.onion", 0, listener2, mainThread);
  } catch (e) {
    // it is ok for this negative test to fail fast
    do_check_true(true);
    do_test_3();
  }
}

function all_done() {
  // reset locally modified prefs
  prefs.setCharPref("network.dns.localDomains", localdomainPref);
  prefs.setBoolPref("network.dns.blockDotOnion", onionPref);
  do_test_finished();
}

function run_test() {
  onionPref = prefs.getBoolPref("network.dns.blockDotOnion");
  localdomainPref = prefs.getCharPref("network.dns.localDomains");
  prefs.setCharPref("network.dns.localDomains", "private.onion");
  dns.asyncResolve("localhost", 0, listener1, mainThread);
  do_test_pending();
}

