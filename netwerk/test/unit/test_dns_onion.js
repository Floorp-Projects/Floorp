var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
var mainThread = threadManager.currentThread;

var onionPref;
var localdomainPref;
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

// check that we don't lookup .onion
var listenerBlock = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    do_check_false(Components.isSuccessCode(inStatus));
    do_test_dontBlock();
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

// check that we do lookup .onion (via pref)
var listenerDontBlock = {
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

function do_test_dontBlock() {
  prefs.setBoolPref("network.dns.blockDotOnion", false);
  dns.asyncResolve("private.onion", 0, listenerDontBlock, mainThread);
}

function do_test_block() {
  prefs.setBoolPref("network.dns.blockDotOnion", true);
  try {
    dns.asyncResolve("private.onion", 0, listenerBlock, mainThread);
  } catch (e) {
    // it is ok for this negative test to fail fast
    do_check_true(true);
    do_test_dontBlock();
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
  do_test_block();
  do_test_pending();
}

