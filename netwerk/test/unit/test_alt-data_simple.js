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

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULRuntime)
           .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

const responseContent = "response body";
const responseContent2 = "response body 2";
const altContent = "!@#$%^&*()";
const altContentType = "text/binary";

var servedNotModified = false;
var shouldPassRevalidation = true;

var cache_storage = null;

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

  if (etag == "test-etag1" && shouldPassRevalidation) {
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
    servedNotModified = true;
  } else {
    var content = shouldPassRevalidation ? responseContent : responseContent2;
    response.bodyOutputStream.write(content, content.length);
  }
}

function check_has_alt_data_in_index(aHasAltData)
{
  if (inChildProcess()) {
    return;
  }
  var hasAltData = {};
  cache_storage.getCacheIndexEntryAttrs(createURI(URL), "", hasAltData, {});
  Assert.equal(hasAltData.value, aHasAltData);
}

function run_test()
{
  do_get_profile();
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);
  do_test_pending();

  if (!inChildProcess()) {
    cache_storage = getCacheStorage("disk") ;
    wait_for_cache_index(asyncOpen);
  } else {
    asyncOpen();
  }
}

function asyncOpen()
{
  var chan = make_channel(URL);

  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(altContentType);

  chan.asyncOpen2(new ChannelListener(readServerContent, null));
}

function readServerContent(request, buffer)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(buffer, responseContent);
  Assert.equal(cc.alternativeDataType, "");
  check_has_alt_data_in_index(false);

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
  openAltChannel();
}};

function flushAndOpenAltChannel()
{
  // We need to do a GC pass to ensure the cache entry has been freed.
  gc();
  if (!inChildProcess()) {
    Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver);
  } else {
    do_send_remote_message('flush');
    do_await_remote_message('flushed').then(() => {
      openAltChannel();
    });
  }
}

function openAltChannel() {
  var chan = make_channel(URL);
  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(altContentType);

  chan.asyncOpen2(new ChannelListener(readAltContent, null));
}

function readAltContent(request, buffer)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(servedNotModified, true);
  Assert.equal(cc.alternativeDataType, altContentType);
  Assert.equal(buffer, altContent);
  check_has_alt_data_in_index(true);

  requestAgain();
}

function requestAgain()
{
  shouldPassRevalidation = false;
  var chan = make_channel(URL);
  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(altContentType);
  chan.asyncOpen2(new ChannelListener(readEmptyAltContent, null));
}

function readEmptyAltContent(request, buffer)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  // the cache is overwrite and the alt-data is reset
  Assert.equal(cc.alternativeDataType, "");
  Assert.equal(buffer, responseContent2);
  check_has_alt_data_in_index(false);

  httpServer.stop(do_test_finished);
}
