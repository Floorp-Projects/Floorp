const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/421";
var httpbody = "0123456789";
var channel;
var ios;

function run_test() {
  setup_test();
  do_test_pending();
}

function setup_test() {
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  channel = setupChannel(testpath);

  channel.asyncOpen(new ChannelListener(checkRequestResponse, channel));
}

function setupChannel(path) {
  var chan = NetUtil.newChannel({uri: URL + path, loadUsingSystemPrincipal: true});
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

var iters = 0;

function serverHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);

  if (!iters) {
    response.setStatusLine("1.1", 421, "Not Authoritative " + iters);
  } else {
    response.setStatusLine("1.1", 200, "OK");
  }
  ++iters;

  response.bodyOutputStream.write(httpbody, httpbody.length);
}

function checkRequestResponse(request, data, context) {
  Assert.equal(channel.responseStatus, 200);
  Assert.equal(channel.responseStatusText, "OK");
  Assert.ok(channel.requestSucceeded);

  Assert.equal(channel.contentType, "text/plain");
  Assert.equal(channel.contentLength, httpbody.length);
  Assert.equal(data, httpbody);

  httpserver.stop(do_test_finished);
}
