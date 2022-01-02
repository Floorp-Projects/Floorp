/*

Checkes if the concurrent cache read/write works when the write is interrupted because of max-entry-size limits.
This is enhancement of 29c test, this test checks that a corrupted 206 response is correctly handled (no crashes or asserion failures)
This test is using a resumable response.
- with a profile, set max-entry-size to 1 (=1024 bytes)
- first channel makes a request for a resumable response
- second channel makes a request for the same resource, concurrent read happens
- first channel sets predicted data size on the entry with every chunk, it's doomed on 1024
- second channel now must engage interrupted concurrent write algorithm and read the rest of the content from the network
- the response to the range request is plain 200
- the first must deliver full content w/o errors
- the second channel must correctly fail

*/
"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpProtocolHandler = Cc[
  "@mozilla.org/network/protocol;1?name=http"
].getService(Ci.nsIHttpProtocolHandler);

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
  response.setHeader("Accept-Ranges", "bytes");
  response.setHeader("Content-Length", "" + responseBody.length);
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

  httpProtocolHandler.EnsureHSTSDataReady().then(function() {
    var chan1 = make_channel(URL + "/content");
    chan1.asyncOpen(new ChannelListener(firstTimeThrough, null));
    var chan2 = make_channel(URL + "/content");
    chan2.asyncOpen(
      new ChannelListener(secondTimeThrough, null, CL_EXPECT_FAILURE)
    );
  });

  do_test_pending();
}

function firstTimeThrough(request, buffer) {
  Assert.equal(buffer, responseBody);
}

function secondTimeThrough(request, buffer) {
  httpServer.stop(do_test_finished);
}
