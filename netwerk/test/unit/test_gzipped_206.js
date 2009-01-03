do_import_script("netwerk/test/httpserver/httpd.js");

var httpserver = null;

const responseBody = [0x1f, 0x8b, 0x08, 0x00, 0x16, 0x5a, 0x8a, 0x48, 0x02,
		      0x03, 0x2b, 0x49, 0x2d, 0x2e, 0xe1, 0x02, 0x00, 0xc6,
		      0x35, 0xb9, 0x3b, 0x05, 0x00, 0x00, 0x00];

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

var doRangeResponse = false;

function cachedHandler(metadata, response) {
  response.setHeader("Content-Type", "application/x-gzip", false);
  response.setHeader("Content-Encoding", "gzip", false);
  response.setHeader("ETag", "Just testing");

  var body = responseBody;

  if (doRangeResponse) {
    do_check_true(metadata.hasHeader("Range"));
    var matches = metadata.getHeader("Range").match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
    var from = (matches[1] === undefined) ? 0 : matches[1];
    var to = (matches[2] === undefined) ? responseBody.length - 1 : matches[2];
    if (from >= responseBody.length) {
      response.setStatusLine(metadata.httpVersion, 416, "Start pos too high");
      response.setHeader("Content-Range", "*/" + responseBody.length, false);
      return;
    }
    body = body.slice(from, to + 1);
    // always respond to successful range requests with 206
    response.setStatusLine(metadata.httpVersion, 206, "Partial Content");
    response.setHeader("Content-Range", from + "-" + to + "/" + responseBody.length, false);
  } else {
    response.setHeader("Accept-Ranges", "bytes");
    doRangeResponse = true;
  }

  var bos = Cc["@mozilla.org/binaryoutputstream;1"]
      .createInstance(Ci.nsIBinaryOutputStream);
  bos.setOutputStream(response.bodyOutputStream);

  bos.writeByteArray(body, body.length);
}

function Canceler() {
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
    continue_test();
  }
};

function continue_test() {
  var chan = make_channel("http://localhost:4444/cached/test.gz");
  chan.asyncOpen(new ChannelListener(finish_test, null), null);
}

function finish_test(request, data, ctx) {
  httpserver.stop();
  do_check_eq(request.status, 0);
  do_check_eq(data.length, responseBody.length);
  for (var i = 0; i < data.length; ++i) {
    do_check_eq(data.charCodeAt(i), responseBody[i]);
  }
  do_test_finished();
}

function run_test() {
  httpserver = new nsHttpServer();
  httpserver.registerPathHandler("/cached/test.gz", cachedHandler);
  httpserver.start(4444);

  var chan = make_channel("http://localhost:4444/cached/test.gz");
  chan.asyncOpen(new Canceler(), null);
  do_test_pending();
}
