"use strict";
// https://bugzilla.mozilla.org/show_bug.cgi?id=761228

do_load_httpd_js();

var httpServer = null;
const testFileName = "test_customConditionalRequest_304";
const basePath = "/" + testFileName + "/";
const baseURI = "http://localhost:4444" + basePath;
const unexpected304 = "unexpected304";
const existingCached304 = "existingCached304";

function make_uri(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newURI(url, null, null);
}

function make_channel(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, null, null).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function clearCache() {
    var service = Components.classes["@mozilla.org/network/cache-service;1"]
        .getService(Ci.nsICacheService);
    service.evictEntries(Ci.nsICache.STORE_ANYWHERE);
}

function alwaysReturn304Handler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
  response.setHeader("Returned-From-Handler", "1");
}

function run_test() {
  evict_cache_entries();

  httpServer = new nsHttpServer();
  httpServer.registerPathHandler(basePath + unexpected304,
                                 alwaysReturn304Handler);
  httpServer.registerPathHandler(basePath + existingCached304,
                                 alwaysReturn304Handler);
  httpServer.start(4444);
  run_next_test();
}

function finish_test(request, buffer) {
  httpServer.stop(do_test_finished);
}

function consume304(request, buffer) {
  do_check_eq(request.responseStatus, 304);
  do_check_eq(request.getResponseHeader("Returned-From-Handler"), "1");
  run_next_test();
}

// Test that we return a 304 response to the caller when we are not expecting
// a 304 response (i.e. when the server shouldn't have sent us one).
add_test(function test_unexpected_304() {
  var chan = make_channel(baseURI + unexpected304);
  chan.asyncOpen(new ChannelListener(consume304, null), null);
});

// Test that we can cope with a 304 response that was (erroneously) stored in
// the cache.
add_test(function test_304_stored_in_cache() {
  asyncOpenCacheEntry(
    baseURI + existingCached304, "HTTP",
    Ci.nsICache.STORE_ANYWHERE, Ci.nsICache.ACCESS_READ_WRITE,
    function (entryStatus, cacheEntry) {
      cacheEntry.setMetaDataElement("request-method", "GET");
      cacheEntry.setMetaDataElement("response-head",
                                    "HTTP/1.1 304 Not Modified\r\n" +
                                    "\r\n");
      cacheEntry.close();

      var chan = make_channel(baseURI + existingCached304);

      // make it a custom conditional request
      chan.QueryInterface(Components.interfaces.nsIHttpChannel);
      chan.setRequestHeader("If-None-Match", '"foo"', false);

      chan.asyncOpen(new ChannelListener(consume304, null), null);
    });
});
