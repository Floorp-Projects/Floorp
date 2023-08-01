/*

Checkes if the concurrent cache read/write works when the write is interrupted because of max-entry-size limits
This test is using a resumable response.
- with a profile, set max-entry-size to 0
- first channel makes a request for a resumable response
- second channel makes a request for the same resource, concurrent read happens
- first channel sets predicted data size on the entry, it's doomed
- second channel now must engage interrupted concurrent write algorithm and read the content again from the network
- both channels must deliver full content w/o errors

*/

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpProtocolHandler = Cc[
  "@mozilla.org/network/protocol;1?name=http"
].getService(Ci.nsIHttpProtocolHandler);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

const responseBody = "response body";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("ETag", "Just testing");
  response.setHeader("Cache-Control", "max-age=99999");
  response.setHeader("Accept-Ranges", "bytes");
  response.setHeader("Content-Length", "" + responseBody.length);
  if (metadata.hasHeader("If-Range")) {
    response.setStatusLine(metadata.httpVersion, 206, "Partial Content");
    response.setHeader("Content-Range", "0-12/13");
  }
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function run_test() {
  do_get_profile();

  Services.prefs.setIntPref("browser.cache.disk.max_entry_size", 0);
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  httpProtocolHandler.EnsureHSTSDataReady().then(function () {
    var chan1 = make_channel(URL + "/content");
    chan1.asyncOpen(new ChannelListener(firstTimeThrough, null));
    var chan2 = make_channel(URL + "/content");
    chan2.asyncOpen(new ChannelListener(secondTimeThrough, null));
  });

  do_test_pending();
}

function firstTimeThrough(request, buffer) {
  Assert.equal(buffer, responseBody);
}

function secondTimeThrough(request, buffer) {
  Assert.equal(buffer, responseBody);
  httpServer.stop(do_test_finished);
}
