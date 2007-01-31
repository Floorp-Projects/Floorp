// This file ensures that canceling a channel early does not
// send the request to the server (bug 350790)

do_import_script("netwerk/test/httpserver/httpd.js");

const NS_BINDING_ABORTED = 0x804b0002;

var observer = {
  QueryInterface: function eventsink_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIObserver))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  observe: function(subject, topic, data) {
    subject = subject.QueryInterface(Components.interfaces.nsIRequest);
    subject.cancel(NS_BINDING_ABORTED);

    var obs = Components.classes["@mozilla.org/observer-service;1"].getService();
    obs = obs.QueryInterface(Components.interfaces.nsIObserverService);
    obs.removeObserver(observer, "http-on-modify-request");
  }
};

var listener = {
  onStartRequest: function test_onStartR(request, ctx) {
    do_check_eq(request.status, NS_BINDING_ABORTED);
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    httpserv.stop();
    do_test_finished();
  }
};

function makeChan(url) {
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel(url, null, null)
                .QueryInterface(Components.interfaces.nsIHttpChannel);

  return chan;
}

var httpserv = null;

function run_test() {
  httpserv = new nsHttpServer();
  httpserv.registerPathHandler("/failtest", failtest);
  httpserv.start(4444);

  var obs = Components.classes["@mozilla.org/observer-service;1"].getService();
  obs = obs.QueryInterface(Components.interfaces.nsIObserverService);
  obs.addObserver(observer, "http-on-modify-request", false);

  var chan = makeChan("http://localhost:4444/failtest");

  chan.asyncOpen(listener, null);

  do_test_pending();
}

// PATHS

// /failtest
function failtest(metadata, response) {
  do_throw("This should not be reached");
}
