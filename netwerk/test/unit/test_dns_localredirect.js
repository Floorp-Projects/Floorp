const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

var listener = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    var answer = inRecord.getNextAddrAsString();
    do_check_true(answer == "127.0.0.1" || answer == "::1");

    prefs.clearUserPref("network.dns.localDomains");

    do_test_finished();
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function run_test() {
  prefs.setCharPref("network.dns.localDomains", "local.vingtetun.org");

  var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
  var mainThread = threadManager.currentThread;
  dns.asyncResolve("local.vingtetun.org", 0, listener, mainThread);

  do_test_pending();
}

