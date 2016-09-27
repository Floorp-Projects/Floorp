/**
 * Test for the "alternative data stream" stored withing a cache entry.
 *
 * - we load a URL with preference for an alt data (check what we get is the raw data,
 *   since there was nothing previously cached)
 * - we store the alt data along the channel (to the cache entry)
 * - we flush the HTTP cache
 * - we reload the same URL using a new channel, again prefering the alt data be loaded
 * - this time the alt data must arive
 */

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + "/content";
});

var httpServer = null;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

const responseContent = "response body";
const altContent = "!@#$%^&*()";
const altContentType = "text/binary";

var servedNotModified = false;

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-cache");
  response.setHeader("ETag", "test-etag1");

  try {
    var etag = metadata.getHeader("If-None-Match");
  } catch(ex) {
    var etag = "";
  }

  if (etag == "test-etag1") {
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
    servedNotModified = true;
  } else {
    response.bodyOutputStream.write(responseContent, responseContent.length);
  }
}

function run_test()
{
  do_get_profile();
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  var chan = make_channel(URL);

  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(altContentType);

  chan.asyncOpen2(new ChannelListener(readServerContent, null));
  do_test_pending();
}

function readServerContent(request, buffer)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  do_check_eq(buffer, responseContent);
  do_check_eq(cc.alternativeDataType, "");

  do_execute_soon(() => {
    var os = cc.openAlternativeOutputStream(altContentType);
    os.write(altContent, altContent.length);
    os.close();

    do_execute_soon(flushAndOpenAltChannel);
  });
}

// needs to be rooted
var cacheFlushObserver = cacheFlushObserver = { observe: function() {
  cacheFlushObserver = null;

  var chan = make_channel(URL);
  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(altContentType);

  chan.asyncOpen2(new ChannelListener(readAltContent, null));
}};

function flushAndOpenAltChannel()
{
  // We need to do a GC pass to ensure the cache entry has been freed.
  gc();
  Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver);
}

function readAltContent(request, buffer)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  do_check_eq(servedNotModified, true);
  do_check_eq(cc.alternativeDataType, altContentType);
  do_check_eq(buffer, altContent);

  httpServer.stop(do_test_finished);
}
