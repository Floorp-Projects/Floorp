/**
 * Test for overwriting the alternative data in a cache entry.
 *
 * - run_test loads a new channel
 * - readServerContent checks the content, and saves alt-data
 * - cacheFlushObserver creates a new channel with "text/binary" alt-data type
 * - readAltContent checks that it gets back alt-data and creates a channel with the dummy/null alt-data type
 * - readServerContent2 checks that it gets regular content, from the cache and tries to overwrite the alt-data with the same representation
 * - cacheFlushObserver2 creates a new channel with "text/binary" alt-data type
 * - readAltContent2 checks that it gets back alt-data, and tries to overwrite with a kind of alt-data
 * - cacheFlushObserver3 creates a new channel with "text/binary2" alt-data type
 * - readAltContent3 checks that it gets back the newly saved alt-data
 */

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + "/content";
});

let httpServer = null;

function make_and_open_channel(url, altContentType, callback) {
  let chan = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
  if (altContentType) {
    let cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
    cc.preferAlternativeDataType(altContentType);
  }
  chan.asyncOpen2(new ChannelListener(callback, null));
}

const responseContent = "response body";
const altContent = "!@#$%^&*()";
const altContentType = "text/binary";
const altContent2 = "abc";
const altContentType2 = "text/binary2";

let servedNotModified = false;

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-cache");
  response.setHeader("ETag", "test-etag1");
  let etag = ""
  try {
    etag = metadata.getHeader("If-None-Match");
  } catch(ex) {
    etag = "";
  }

  if (etag == "test-etag1") {
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
    servedNotModified = true;
  } else {
    servedNotModified = false;
    response.bodyOutputStream.write(responseContent, responseContent.length);
  }
}

function run_test()
{
  do_get_profile();
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  do_test_pending();
  make_and_open_channel(URL, altContentType, readServerContent);
}

function readServerContent(request, buffer)
{
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  do_check_eq(buffer, responseContent);
  do_check_eq(cc.alternativeDataType, "");

  do_execute_soon(() => {
    let os = cc.openAlternativeOutputStream(altContentType);
    os.write(altContent, altContent.length);
    os.close();

    do_execute_soon(flushAndOpenAltChannel);
  });
}

function flushAndOpenAltChannel()
{
  // We need to do a GC pass to ensure the cache entry has been freed.
  Cu.forceShrinkingGC();
  Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver);
}

// needs to be rooted
let cacheFlushObserver = { observe: function() {
  if (!cacheFlushObserver) {
    do_print("ignoring cacheFlushObserver\n");
    return;
  }
  cacheFlushObserver = null;
  Cu.forceShrinkingGC();
  make_and_open_channel(URL, altContentType, readAltContent);
}};

function readAltContent(request, buffer, closure, fromCache)
{
  Cu.forceShrinkingGC();
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  do_check_eq(fromCache || servedNotModified, true);
  do_check_eq(cc.alternativeDataType, altContentType);
  do_check_eq(buffer, altContent);

  make_and_open_channel(URL, "dummy/null", readServerContent2);
}

function readServerContent2(request, buffer, closure, fromCache)
{
  Cu.forceShrinkingGC();
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  do_check_eq(fromCache || servedNotModified, true);
  do_check_eq(buffer, responseContent);
  do_check_eq(cc.alternativeDataType, "");

  do_execute_soon(() => {
    let os = cc.openAlternativeOutputStream(altContentType);
    os.write(altContent, altContent.length);
    os.close();

    do_execute_soon(flushAndOpenAltChannel2);
  });
}

function flushAndOpenAltChannel2()
{
  // We need to do a GC pass to ensure the cache entry has been freed.
  Cu.forceShrinkingGC();
  Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver2);
}

// needs to be rooted
let cacheFlushObserver2 = { observe: function() {
  if (!cacheFlushObserver2) {
    do_print("ignoring cacheFlushObserver2\n");
    return;
  }
  cacheFlushObserver2 = null;
  Cu.forceShrinkingGC();
  make_and_open_channel(URL, altContentType, readAltContent2);
}};

function readAltContent2(request, buffer, closure, fromCache)
{
  Cu.forceShrinkingGC();
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  do_check_eq(servedNotModified || fromCache, true);
  do_check_eq(cc.alternativeDataType, altContentType);
  do_check_eq(buffer, altContent);

  do_execute_soon(() => {
    Cu.forceShrinkingGC();
    do_print("writing other content\n");
    let os = cc.openAlternativeOutputStream(altContentType2);
    os.write(altContent2, altContent2.length);
    os.close();

    do_execute_soon(flushAndOpenAltChannel3);
  });
}

function flushAndOpenAltChannel3()
{
  // We need to do a GC pass to ensure the cache entry has been freed.
  Cu.forceShrinkingGC();
  Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver3);
}

// needs to be rooted
let cacheFlushObserver3 = { observe: function() {
  if (!cacheFlushObserver3) {
    do_print("ignoring cacheFlushObserver3\n");
    return;
  }

  cacheFlushObserver3 = null;
  Cu.forceShrinkingGC();
  make_and_open_channel(URL, altContentType2, readAltContent3);
}};


function readAltContent3(request, buffer, closure, fromCache)
{
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  do_check_eq(servedNotModified || fromCache, true);
  do_check_eq(cc.alternativeDataType, altContentType2);
  do_check_eq(buffer, altContent2);

  httpServer.stop(do_test_finished);
}
