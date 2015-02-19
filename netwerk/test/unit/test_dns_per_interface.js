var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);

// This test checks DNSService host resolver when a network interface is supplied
// as well. In the test 3 request are sent: two with a network interface set 
// and one without a network interface.
// All requests have the same host to be resolved and the same flags.
// One of the request with the network interface will be canceled.
// The request with and without a network interface should not be mixed during
// the requests lifetime.

var netInterface1 = "interface1";
var netInterface2 = "interface2";

// We are not using localhost because on e10s a host resolve callback is almost
// always faster than a cancel request, therefore cancel operation would not be
// tested.
var hostname = "thisshouldnotexist.mozilla.com";

// 3 requests.
var requestWithInterfaceCanceled;
var requestWithoutInterfaceNotCanceled;
var requestWithInterfaceNotCanceled;

var listener = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    // Two requests should be resolved and one request should be canceled.
    // Since cancalation of a request is racy we will check only for not
    // canceled request - they should not be canceled.
    if ((inRequest == requestWithoutInterfaceNotCanceled) ||
        (inRequest == requestWithInterfaceNotCanceled)) {
      // This request should not be canceled.
      do_check_neq(inStatus, Cr.NS_ERROR_ABORT);

      do_test_finished();
    } else if (inRequest == requestWithInterfaceCanceled) {
      // We do not check the outcome for this one because it is racy -
      // whether the request cancelation is faster than resolving the request.
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
  var threadManager = Cc["@mozilla.org/thread-manager;1"]
                        .getService(Ci.nsIThreadManager);
  var mainThread = threadManager.currentThread;

  var flags = Ci.nsIDNSService.RESOLVE_BYPASS_CACHE;

  // This one will be canceled.
  requestWithInterfaceCanceled = dns.asyncResolveExtended(hostname, flags,
                                                          netInterface1,
                                                          listener,
                                                          mainThread);
  requestWithInterfaceCanceled.cancel(Cr.NS_ERROR_ABORT);

  // This one will not be canceled. This is the request without a network
  // interface.
  requestWithoutInterfaceNotCanceled = dns.asyncResolve(hostname, flags,
                                                        listener, mainThread);

  // This one will not be canceled.
  requestWithInterfaceNotCanceled = dns.asyncResolveExtended(hostname, flags,
                                                             netInterface2,
                                                             listener,
                                                             mainThread);
  // We wait for notifications for the requests.
  // For each request onLookupComplete will be called.
  do_test_pending();
  do_test_pending();
  do_test_pending();
}
