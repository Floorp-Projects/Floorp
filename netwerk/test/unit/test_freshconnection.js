// This is essentially a debug mode crashtest to make sure everything
// involved in a reload runs on the right thread. It relies on the
// assertions in necko.

var listener = {
  onStartRequest: function test_onStartR(request) {
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    do_test_finished();
  },
};

function run_test() {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:4444",
    loadUsingSystemPrincipal: true
  });
  chan.loadFlags = Ci.nsIRequest.LOAD_FRESH_CONNECTION |
	           Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.asyncOpen(listener);
  do_test_pending();
}

