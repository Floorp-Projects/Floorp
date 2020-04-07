// Test bug 1527293
//
// Summary:
// The purpose of this test is to check that a cache entry is doomed and not
// reused when we don't write the content due to max entry size limit.
//
// Test step:
// 1. Create http request for an entry whose size is bigger than we allow to
//    cache. The response must contain Content-Range header so the content size
//    is known in advance, but it must not contain Content-Length header because
//    the bug isn't reproducible with it.
// 2. After receiving and checking the content do the same request again.
// 3. Check that the request isn't conditional, i.e. the entry from previous
//    load was doomed.

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

// need something bigger than 1024 bytes
const responseBody =
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" +
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("ETag", "Just testing");
  response.setHeader("Cache-Control", "max-age=99999");

  Assert.throws(
    () => {
      var etag = metadata.getHeader("If-None-Match");
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "conditional request not expected"
  );

  response.setHeader("Accept-Ranges", "bytes");
  let len = responseBody.length;
  response.setHeader("Content-Range", "0-" + (len - 1) + "/" + len);
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function run_test() {
  // Static check
  Assert.ok(responseBody.length > 1024);

  do_get_profile();

  Services.prefs.setIntPref("browser.cache.disk.max_entry_size", 1);
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  var chan = make_channel(URL + "/content");
  chan.asyncOpen(new ChannelListener(firstTimeThrough, null));

  do_test_pending();
}

function firstTimeThrough(request, buffer) {
  Assert.equal(buffer, responseBody);

  var chan = make_channel(URL + "/content");
  chan.asyncOpen(new ChannelListener(secondTimeThrough, null));
}

function secondTimeThrough(request, buffer) {
  Assert.equal(buffer, responseBody);
  httpServer.stop(do_test_finished);
}
