/* Test to ensure our 64-bit content length implementation works, at least for
   a simple HTTP case */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

// This C-L is significantly larger than (U)INT32_MAX, to make sure we do
// 64-bit properly.
const CONTENT_LENGTH = "1152921504606846975";

var httpServer = null;

var listener = {
  onStartRequest(req) {},

  onDataAvailable(req, stream, off, count) {
    Assert.equal(req.getResponseHeader("Content-Length"), CONTENT_LENGTH);

    // We're done here, cancel the channel
    req.cancel(Cr.NS_BINDING_ABORTED);
  },

  onStopRequest(req, stat) {
    httpServer.stop(do_test_finished);
  },
};

function hugeContentLength(metadata, response) {
  var text = "abcdefghijklmnopqrstuvwxyz";
  var bytes_written = 0;

  response.seizePower();

  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Length: " + CONTENT_LENGTH + "\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");

  // Write enough data to ensure onDataAvailable gets called
  while (bytes_written < 4096) {
    response.write(text);
    bytes_written += text.length;
  }

  response.finish();
}

function test_hugeContentLength() {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpServer.identity.primaryPort + "/",
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.asyncOpen(listener);
}

add_test(test_hugeContentLength);

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/", hugeContentLength);
  httpServer.start(-1);
  run_next_test();
}
