/*

Checkes if the concurrent cache read/write works when the write is interrupted because of max-entry-size limits.
This test is using a non-resumable response.
- with a profile, set max-entry-size to 0
- first channel makes a request for a non-resumable (chunked) response
- second channel makes a request for the same resource, concurrent read is bypassed (non-resumable response)
- first channel writes first bytes to the cache output stream, but that fails because of the max-entry-size limit and entry is doomed
- cache entry output stream is closed
- second channel gets the entry, opening the input stream must fail
- second channel must read the content again from the network
- both channels must deliver full content w/o errors

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

const responseBody = "c\r\ndata reached\r\n3\r\nhej\r\n0\r\n\r\n";
const responseBodyDecoded = "data reachedhej";

function contentHandler(metadata, response) {
  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(responseBody);
  response.finish();
}

function run_test() {
  do_get_profile();

  Services.prefs.setIntPref("browser.cache.disk.max_entry_size", 0);
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  httpProtocolHandler.EnsureHSTSDataReady().then(function() {
    var chan1 = make_channel(URL + "/content");
    chan1.asyncOpen(
      new ChannelListener(firstTimeThrough, null, CL_ALLOW_UNKNOWN_CL)
    );
    var chan2 = make_channel(URL + "/content");
    chan2.asyncOpen(
      new ChannelListener(secondTimeThrough, null, CL_ALLOW_UNKNOWN_CL)
    );
  });

  do_test_pending();
}

function firstTimeThrough(request, buffer) {
  Assert.equal(buffer, responseBodyDecoded);
}

function secondTimeThrough(request, buffer) {
  Assert.equal(buffer, responseBodyDecoded);
  httpServer.stop(do_test_finished);
}
