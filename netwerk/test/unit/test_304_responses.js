"use strict";
// https://bugzilla.mozilla.org/show_bug.cgi?id=761228

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;
const testFileName = "test_customConditionalRequest_304";
const basePath = "/" + testFileName + "/";

XPCOMUtils.defineLazyGetter(this, "baseURI", function() {
  return URL + basePath;
});

const unexpected304 = "unexpected304";
const existingCached304 = "existingCached304";

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function alwaysReturn304Handler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
  response.setHeader("Returned-From-Handler", "1");
}

function run_test() {
  evict_cache_entries();

  httpServer = new HttpServer();
  httpServer.registerPathHandler(
    basePath + unexpected304,
    alwaysReturn304Handler
  );
  httpServer.registerPathHandler(
    basePath + existingCached304,
    alwaysReturn304Handler
  );
  httpServer.start(-1);
  run_next_test();
}

function consume304(request, buffer) {
  request.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(request.responseStatus, 304);
  Assert.equal(request.getResponseHeader("Returned-From-Handler"), "1");
  run_next_test();
}

// Test that we return a 304 response to the caller when we are not expecting
// a 304 response (i.e. when the server shouldn't have sent us one).
add_test(function test_unexpected_304() {
  var chan = make_channel(baseURI + unexpected304);
  chan.asyncOpen(new ChannelListener(consume304, null));
});

// Test that we can cope with a 304 response that was (erroneously) stored in
// the cache.
add_test(function test_304_stored_in_cache() {
  asyncOpenCacheEntry(
    baseURI + existingCached304,
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    function(entryStatus, cacheEntry) {
      cacheEntry.setMetaDataElement("request-method", "GET");
      cacheEntry.setMetaDataElement(
        "response-head",
        // eslint-disable-next-line no-useless-concat
        "HTTP/1.1 304 Not Modified\r\n" + "\r\n"
      );
      cacheEntry.metaDataReady();
      cacheEntry.close();

      var chan = make_channel(baseURI + existingCached304);

      // make it a custom conditional request
      chan.QueryInterface(Ci.nsIHttpChannel);
      chan.setRequestHeader("If-None-Match", '"foo"', false);

      chan.asyncOpen(new ChannelListener(consume304, null));
    }
  );
});
