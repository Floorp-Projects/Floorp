// This is essentially a crashtest for accessing an out of range port
// Perform the async open several times in order to induce exponential
// scheduling behavior bugs.

Cu.import("resource://gre/modules/NetUtil.jsm");

var CC = Components.Constructor;

var counter = 0;
const iterations = 10;

var listener = {
  onStartRequest: function test_onStartR(request, ctx) {
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    if (counter++ == iterations)
      do_test_finished();
    else
      execute_test();
  },
};

function run_test() {
  execute_test();
  do_test_pending();
}

function execute_test() {
  var chan = NetUtil.newChannel({uri: "http://localhost:75000", loadUsingSystemPrincipal: true});
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.asyncOpen2(listener);
}

