/**
 * Check how nsICachingChannel.cacheOnlyMetadata works.
 * - all channels involved in this test are set cacheOnlyMetadata = true
 * - do a previously uncached request for a long living content
 * - check we have downloaded the content from the server (channel provides it)
 * - check the entry has metadata, but zero-length content
 * - load the same URL again, now cached
 * - check the channel is giving no content (no call to OnDataAvailable) but succeeds
 * - repeat again, but for a different URL that is not cached (immediately expires)
 * - only difference is that we get a newer version of the content from the server during the second request
 */

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

const responseBody1 = "response body 1";
const responseBody2a = "response body 2a";
const responseBody2b = "response body 2b";

function contentHandler1(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-control", "max-age=999999");
  response.bodyOutputStream.write(responseBody1, responseBody1.length);
}

var content2passCount = 0;

function contentHandler2(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-control", "no-cache");
  switch (content2passCount++) {
    case 0:
      response.setHeader("ETag", "testetag");
      response.bodyOutputStream.write(responseBody2a, responseBody2a.length);
      break;
    case 1:
      Assert.ok(metadata.hasHeader("If-None-Match"));
      Assert.equal(metadata.getHeader("If-None-Match"), "testetag");
      response.bodyOutputStream.write(responseBody2b, responseBody2b.length);
      break;
    default:
      throw new Error("Unexpected request in the test");
  }
}

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content1", contentHandler1);
  httpServer.registerPathHandler("/content2", contentHandler2);
  httpServer.start(-1);

  run_test_content1a();
  do_test_pending();
}

function run_test_content1a() {
  var chan = make_channel(URL + "/content1");
  let caching = chan.QueryInterface(Ci.nsICachingChannel);
  caching.cacheOnlyMetadata = true;
  chan.asyncOpen(new ChannelListener(contentListener1a, null));
}

function contentListener1a(request, buffer) {
  Assert.equal(buffer, responseBody1);

  asyncOpenCacheEntry(URL + "/content1", "disk", 0, null, cacheCheck1);
}

function cacheCheck1(status, entry) {
  Assert.equal(status, 0);
  Assert.equal(entry.dataSize, 0);
  try {
    Assert.notEqual(entry.getMetaDataElement("response-head"), null);
  } catch (ex) {
    do_throw("Missing response head");
  }

  var chan = make_channel(URL + "/content1");
  let caching = chan.QueryInterface(Ci.nsICachingChannel);
  caching.cacheOnlyMetadata = true;
  chan.asyncOpen(new ChannelListener(contentListener1b, null, CL_IGNORE_CL));
}

function contentListener1b(request, buffer) {
  request.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(request.requestMethod, "GET");
  Assert.equal(request.responseStatus, 200);
  Assert.equal(request.getResponseHeader("Cache-control"), "max-age=999999");

  Assert.equal(buffer, "");
  run_test_content2a();
}

// Now same set of steps but this time for an immediately expiring content.

function run_test_content2a() {
  var chan = make_channel(URL + "/content2");
  let caching = chan.QueryInterface(Ci.nsICachingChannel);
  caching.cacheOnlyMetadata = true;
  chan.asyncOpen(new ChannelListener(contentListener2a, null));
}

function contentListener2a(request, buffer) {
  Assert.equal(buffer, responseBody2a);

  asyncOpenCacheEntry(URL + "/content2", "disk", 0, null, cacheCheck2);
}

function cacheCheck2(status, entry) {
  Assert.equal(status, 0);
  Assert.equal(entry.dataSize, 0);
  try {
    Assert.notEqual(entry.getMetaDataElement("response-head"), null);
    Assert.ok(
      entry.getMetaDataElement("response-head").match("etag: testetag")
    );
  } catch (ex) {
    do_throw("Missing response head");
  }

  var chan = make_channel(URL + "/content2");
  let caching = chan.QueryInterface(Ci.nsICachingChannel);
  caching.cacheOnlyMetadata = true;
  chan.asyncOpen(new ChannelListener(contentListener2b, null));
}

function contentListener2b(request, buffer) {
  Assert.equal(buffer, responseBody2b);

  httpServer.stop(do_test_finished);
}
