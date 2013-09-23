var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);

var listener = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    var answer = inRecord.getNextAddrAsString();
    do_check_true(answer == "127.0.0.1" || answer == "::1");

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
  var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
  var mainThread = threadManager.currentThread;
  dns.asyncResolve("localhost", 0, listener, mainThread);

  do_test_pending();
}

