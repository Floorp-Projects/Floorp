var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);

var hostname1 = "";
var hostname2 = "";
var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

for( var i=0; i < 20; i++ ) {
  hostname1 += possible.charAt(Math.floor(Math.random() * possible.length));
  hostname2 += possible.charAt(Math.floor(Math.random() * possible.length));
}

var requestList1Canceled1;
var requestList1Canceled2;
var requestList1NotCanceled;

var requestList2Canceled;
var requestList2NotCanceled;

var listener1 = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    // One request should be resolved and two request should be canceled.
    if (inRequest == requestList1NotCanceled) {
      // This request should not be canceled.
      do_check_neq(inStatus, Cr.NS_ERROR_ABORT);

      do_test_finished();
    }
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
    // One request should be resolved and the other canceled.
    if (inRequest == requestList2NotCanceled) {
      // The request should not be canceled.
      do_check_neq(inStatus, Cr.NS_ERROR_ABORT);

      do_test_finished();
    }
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

  var flags = Ci.nsIDNSService.RESOLVE_BYPASS_CACHE;

  // This one will be canceled with cancelAsyncResolve.
  requestList1Canceled1 = dns.asyncResolve(hostname2, flags, listener1, mainThread);
  dns.cancelAsyncResolve(hostname2, flags, listener1, Cr.NS_ERROR_ABORT);

  // This one will not be canceled.
  requestList1NotCanceled = dns.asyncResolve(hostname1, flags, listener1, mainThread);

  // This one will be canceled with cancel(Cr.NS_ERROR_ABORT).
  requestList1Canceled2 = dns.asyncResolve(hostname1, flags, listener1, mainThread);
  requestList1Canceled2.cancel(Cr.NS_ERROR_ABORT);

  // This one will not be canceled.
  requestList2NotCanceled = dns.asyncResolve(hostname1, flags, listener2, mainThread);

  // This one will be canceled with cancel(Cr.NS_ERROR_ABORT).
  requestList2Canceled = dns.asyncResolve(hostname2, flags, listener2, mainThread);
  requestList2Canceled.cancel(Cr.NS_ERROR_ABORT);

  do_test_pending();
  do_test_pending();
}
