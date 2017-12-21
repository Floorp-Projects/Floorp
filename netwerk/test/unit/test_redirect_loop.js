Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

/*
 * This xpcshell test checks whether we detect infinite HTTP redirect loops.
 * We check loops with "Location:" set to 1) full URI, 2) relative URI, and 3)
 * empty Location header (which resolves to a relative link to the original
 * URI when the original URI ends in a slash).
 */

var httpServer = new HttpServer();
httpServer.start(-1);
const PORT = httpServer.identity.primaryPort;

var fullLoopPath = "/fullLoop"; 
var fullLoopURI = "http://localhost:" + PORT + fullLoopPath;

var relativeLoopPath = "/relativeLoop"; 
var relativeLoopURI = "http://localhost:" + PORT + relativeLoopPath;

// must use directory-style URI, so empty Location redirects back to self
var emptyLoopPath = "/empty/";
var emptyLoopURI = "http://localhost:" + PORT + emptyLoopPath;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

function fullLoopHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.setHeader("Location", "http://localhost:" + PORT + "/fullLoop", false);
}

function relativeLoopHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.setHeader("Location", "relativeLoop", false);
}

function emptyLoopHandler(metadata, response)
{
  // Comrades!  We must seize power from the petty-bourgeois running dogs of
  // httpd.js in order to reply with a blank Location header!
  response.seizePower();
  response.write("HTTP/1.0 301 Moved\r\n");
  response.write("Location: \r\n");
  response.write("Content-Length: 4\r\n");
  response.write("\r\n");
  response.write("oops");
  response.finish();
}

function testFullLoop(request, buffer)
{
  Assert.equal(request.status, Components.results.NS_ERROR_REDIRECT_LOOP);

  var chan = make_channel(relativeLoopURI);
  chan.asyncOpen2(new ChannelListener(testRelativeLoop, null, CL_EXPECT_FAILURE));
}

function testRelativeLoop(request, buffer)
{
  Assert.equal(request.status, Components.results.NS_ERROR_REDIRECT_LOOP);

  var chan = make_channel(emptyLoopURI);
  chan.asyncOpen2(new ChannelListener(testEmptyLoop, null, CL_EXPECT_FAILURE));
}

function testEmptyLoop(request, buffer)
{
  Assert.equal(request.status, Components.results.NS_ERROR_REDIRECT_LOOP);

  httpServer.stop(do_test_finished);
}

function run_test()
{
  httpServer.registerPathHandler(fullLoopPath, fullLoopHandler);
  httpServer.registerPathHandler(relativeLoopPath, relativeLoopHandler);
  httpServer.registerPathHandler(emptyLoopPath, emptyLoopHandler);

  var chan = make_channel(fullLoopURI);
  chan.asyncOpen2(new ChannelListener(testFullLoop, null, CL_EXPECT_FAILURE));
  do_test_pending();
}
