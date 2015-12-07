// Regression test for bug 407303 - A failed channel should not be checked
// for an unsafe content type.

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

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
  Services.prefs.setBoolPref("network.jar.block-remote-files", false);
  var channel = NetUtil.newChannel({
    uri: "jar:http://test.invalid/test.jar!/index.html",
    loadUsingSystemPrincipal: true}
  );
  channel.asyncOpen2(listener);
  do_test_pending();
}
