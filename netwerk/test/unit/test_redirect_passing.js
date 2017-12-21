Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;
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
  chan.asyncOpen2(new ChannelListener(finish_test, null));
}

function finish_test(request, buffer)
{
  Assert.equal(buffer, responseBody);
  httpServer.stop(do_test_finished);
}

function run_test()
{
  httpServer = new HttpServer();
  httpServer.registerPathHandler(randomPath, redirectHandler);
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  var chan = make_channel(randomURI);
  chan.asyncOpen2(new ChannelListener(firstTimeThrough, null));
  do_test_pending();
}
