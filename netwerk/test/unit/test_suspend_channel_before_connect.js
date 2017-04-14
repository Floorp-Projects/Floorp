
var CC = Components.Constructor;

Cu.import("resource://gre/modules/NetUtil.jsm");

const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init");

var obs = Cc["@mozilla.org/observer-service;1"]
            .getService(Ci.nsIObserverService);

var ios = Cc["@mozilla.org/network/io-service;1"]
            .getService(Components.interfaces.nsIIOService);

// A server that waits for a connect. If a channel is suspended it should not
// try to connect to the server until it is is resumed or not try at all if it
// is cancelled as in this test.
function TestServer() {
  this.listener = ServerSocket(-1, true, -1);
  this.port = this.listener.port;
  this.listener.asyncListen(this);
}

TestServer.prototype = {
  onSocketAccepted: function(socket, trans) {
    do_check_true(false, "Socket should not have tried to connect!");
  },

  onStopListening: function(socket) {
  },

  stop: function() {
    try { this.listener.close(); } catch(ignore) {}
  }
}

var requestListenerObserver = {

  QueryInterface: function queryinterface(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIObserver))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  observe: function(subject, topic, data) {
    if (topic === "http-on-modify-request" &&
        subject instanceof Ci.nsIHttpChannel) {

      var chan = subject.QueryInterface(Ci.nsIHttpChannel);
      chan.suspend();
      var obs = Cc["@mozilla.org/observer-service;1"].getService();
      obs = obs.QueryInterface(Ci.nsIObserverService);
      obs.removeObserver(this, "http-on-modify-request");

      // Timers are bad, but we need to wait to see that we are not trying to
      // connect to the server. There are no other event since nothing should
      // happen until we resume the channel.
      let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      timer.initWithCallback(() => {
        chan.cancel(Cr.NS_BINDING_ABORTED);
        chan.resume();
      }, 1000, Ci.nsITimer.TYPE_ONE_SHOT);
    }
  }
};

var listener = {
  onStartRequest: function test_onStartR(request, ctx) {
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    do_execute_soon(run_next_test);
  }
};

// Add observer and start a channel. Observer is going to suspend the channel on
// "http-on-modify-request" even. If a channel is suspended so early it should
// not try to connect at all until it is resumed. In this case we are going to
// wait for some time and cancel the channel before resuming it.
add_test(function testNoConnectChannelCanceledEarly() {

  serv = new TestServer();

  obs.addObserver(requestListenerObserver, "http-on-modify-request", false);
  var chan = NetUtil.newChannel({
    uri:"http://localhost:" + serv.port,
    loadUsingSystemPrincipal: true
  });
  chan.asyncOpen2(listener);

  do_register_cleanup(function(){ serv.stop(); });
});

function run_test() {
  run_next_test();
}
