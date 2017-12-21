Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = null;
// Need to randomize, because apparently no one clears our cache
var randomPath = "/redirect/" + Math.random();

XPCOMUtils.defineLazyGetter(this, "randomURI", function() {
  return URL + randomPath;
});

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

const responseBody = "response body";

function redirectHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.setHeader("Location", URL + "/content", false);
  return;
}

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function firstTimeThrough(request, buffer)
{
  Assert.equal(buffer, responseBody);
  var chan = make_channel(randomURI);
  chan.loadFlags |= Ci.nsIRequest.LOAD_FROM_CACHE;
  chan.asyncOpen2(new ChannelListener(finish_test, null));
}

function finish_test(request, buffer)
{
  Assert.equal(buffer, responseBody);
  httpserver.stop(do_test_finished);
}

function run_test()
{
  httpserver = new HttpServer();
  httpserver.registerPathHandler(randomPath, redirectHandler);
  httpserver.registerPathHandler("/content", contentHandler);
  httpserver.start(-1);

  var chan = make_channel(randomURI);
  chan.asyncOpen2(new ChannelListener(firstTimeThrough, null));
  do_test_pending();
}
