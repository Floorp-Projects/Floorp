Cu.import("resource://testing-common/httpd.js");

var httpserver = null;

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

const responseBody = "response body";

function cachedHandler(metadata, response) {
  var body = responseBody;
  if (metadata.hasHeader("Range")) {
    var matches = metadata.getHeader("Range").match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
    var from = (matches[1] === undefined) ? 0 : matches[1];
    var to = (matches[2] === undefined) ? responseBody.length - 1 : matches[2];
    if (from >= responseBody.length) {
      response.setStatusLine(metadata.httpVersion, 416, "Start pos too high");
      response.setHeader("Content-Range", "*/" + responseBody.length, false);
      return;
    }
    body = responseBody.slice(from, to + 1);
    // always respond to successful range requests with 206
    response.setStatusLine(metadata.httpVersion, 206, "Partial Content");
    response.setHeader("Content-Range", from + "-" + to + "/" + responseBody.length, false);
  }

  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("ETag", "Just testing");
  response.setHeader("Accept-Ranges", "bytes");

  response.bodyOutputStream.write(body, body.length);
}

function Canceler(continueFn) {
  this.continueFn = continueFn;
}

Canceler.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) {
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    request.QueryInterface(Ci.nsIChannel)
           .cancel(Components.results.NS_BINDING_ABORTED);
  },

  onStopRequest: function(request, context, status) {
    do_check_eq(status, Components.results.NS_BINDING_ABORTED);
    this.continueFn();
  }
};

function finish_test() {
  httpserver.stop(do_test_finished);
}

function start_cache_read() {
  var chan = make_channel("http://localhost:" +
                          httpserver.identity.primaryPort + "/cached/test.gz");
  chan.asyncOpen(new ChannelListener(finish_test, null), null);
}

function start_canceler() {
  var chan = make_channel("http://localhost:" +
                          httpserver.identity.primaryPort + "/cached/test.gz");
  chan.asyncOpen(new Canceler(start_cache_read), null);
}

function run_test() {
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/cached/test.gz", cachedHandler);
  httpserver.start(-1);

  var chan = make_channel("http://localhost:" +
                          httpserver.identity.primaryPort + "/cached/test.gz");
  chan.asyncOpen(new ChannelListener(start_canceler, null), null);
  do_test_pending();
}
