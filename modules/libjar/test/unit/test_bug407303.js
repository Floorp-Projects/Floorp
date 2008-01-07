// Regression test for bug 407303 - A failed channel should not be checked
// for an unsafe content type.

const Cc = Components.classes;
const Ci = Components.interfaces;

// XXX: NS_ERROR_UNKNOWN_HOST is not in Components.results
const NS_ERROR_UNKNOWN_HOST = 0x804B001E;

var listener = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIRequestObserver))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) {
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    do_throw("shouldn't get data!");
  },

  onStopRequest: function(request, context, status) {
    do_check_eq(status, NS_ERROR_UNKNOWN_HOST);
    do_test_finished();
  }
};

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);

  var channel = ios.newChannel("jar:http://test.invalid/test.jar!/index.html",
                               null, null);
  channel.asyncOpen(listener, null);
  do_test_pending();
}
