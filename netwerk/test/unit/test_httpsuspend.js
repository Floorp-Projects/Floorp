// This file ensures that suspending a channel directly after opening it
// suspends future notifications correctly.

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

const MIN_TIME_DIFFERENCE = 3000;
const RESUME_DELAY = 5000;

var listener = {
  _lastEvent: 0,
  _gotData: false,

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, ctx) {
    this._lastEvent = Date.now();
    request.QueryInterface(Ci.nsIRequest);

    // Insert a delay between this and the next callback to ensure message buffering
    // works correctly
    request.suspend();
    request.suspend();
    do_timeout(RESUME_DELAY, function() { request.resume(); });
    do_timeout(RESUME_DELAY + 1000, function() { request.resume(); });
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    Assert.ok(Date.now() - this._lastEvent >= MIN_TIME_DIFFERENCE);
    read_stream(stream, count);

    // Ensure that suspending and resuming inside a callback works correctly
    request.suspend();
    request.suspend();
    request.resume();
    request.resume();

    this._gotData = true;
  },

  onStopRequest: function(request, ctx, status) {
    Assert.ok(this._gotData);
    httpserv.stop(do_test_finished);
  }
};

function makeChan(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                .QueryInterface(Ci.nsIHttpChannel);
}

var httpserv = null;

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/woo", data);
  httpserv.start(-1);

  var chan = makeChan(URL + "/woo");
  chan.QueryInterface(Ci.nsIRequest);
  chan.asyncOpen2(listener);

  do_test_pending();
}

function data(metadata, response) {
  let httpbody = "0123456789";
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}
