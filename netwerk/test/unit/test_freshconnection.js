// This is essentially a debug mode crashtest to make sure everything
// involved in a reload runs on the right thread. It relies on the
// assertions in necko.

var listener = {
  onStartRequest: function test_onStartR(request, ctx) {
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    do_test_finished();
  },
};

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:4444", "", null);
  chan.loadFlags = Ci.nsIRequest.LOAD_FRESH_CONNECTION |
	           Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.asyncOpen(listener, null);
  do_test_pending();
}

