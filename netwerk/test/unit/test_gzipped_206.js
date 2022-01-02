"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = null;

// testString = "This is a slightly longer test\n";
const responseBody = [
  0x1f,
  0x8b,
  0x08,
  0x08,
  0xef,
  0x70,
  0xe6,
  0x4c,
  0x00,
  0x03,
  0x74,
  0x65,
  0x78,
  0x74,
  0x66,
  0x69,
  0x6c,
  0x65,
  0x2e,
  0x74,
  0x78,
  0x74,
  0x00,
  0x0b,
  0xc9,
  0xc8,
  0x2c,
  0x56,
  0x00,
  0xa2,
  0x44,
  0x85,
  0xe2,
  0x9c,
  0xcc,
  0xf4,
  0x8c,
  0x92,
  0x9c,
  0x4a,
  0x85,
  0x9c,
  0xfc,
  0xbc,
  0xf4,
  0xd4,
  0x22,
  0x85,
  0x92,
  0xd4,
  0xe2,
  0x12,
  0x2e,
  0x2e,
  0x00,
  0x00,
  0xe5,
  0xe6,
  0xf0,
  0x20,
  0x00,
  0x00,
  0x00,
];

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

var doRangeResponse = false;

function cachedHandler(metadata, response) {
  response.setHeader("Content-Type", "application/x-gzip", false);
  response.setHeader("Content-Encoding", "gzip", false);
  response.setHeader("ETag", "Just testing");
  response.setHeader("Cache-Control", "max-age=3600000"); // avoid validation

  var body = responseBody;

  if (doRangeResponse) {
    Assert.ok(metadata.hasHeader("Range"));
    var matches = metadata
      .getHeader("Range")
      .match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
    var from = matches[1] === undefined ? 0 : matches[1];
    var to = matches[2] === undefined ? responseBody.length - 1 : matches[2];
    if (from >= responseBody.length) {
      response.setStatusLine(metadata.httpVersion, 416, "Start pos too high");
      response.setHeader("Content-Range", "*/" + responseBody.length, false);
      return;
    }
    body = body.slice(from, to + 1);
    response.setHeader("Content-Length", "" + (to + 1 - from));
    // always respond to successful range requests with 206
    response.setStatusLine(metadata.httpVersion, 206, "Partial Content");
    response.setHeader(
      "Content-Range",
      from + "-" + to + "/" + responseBody.length,
      false
    );
  } else {
    // This response will get cut off prematurely
    response.setHeader("Content-Length", "" + responseBody.length);
    response.setHeader("Accept-Ranges", "bytes");
    body = body.slice(0, 17); // slice off a piece to send first
    doRangeResponse = true;
  }

  var bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream
  );
  bos.setOutputStream(response.bodyOutputStream);

  response.processAsync();
  bos.writeByteArray(body);
  response.finish();
}

function continue_test(request, data) {
  Assert.equal(17, data.length);
  var chan = make_channel(
    "http://localhost:" + httpserver.identity.primaryPort + "/cached/test.gz"
  );
  chan.asyncOpen(new ChannelListener(finish_test, null, CL_EXPECT_GZIP));
}

var enforcePref;
var clearBogusContentEncodingPref;

function finish_test(request, data, ctx) {
  Assert.equal(request.status, 0);
  Assert.equal(data.length, responseBody.length);
  for (var i = 0; i < data.length; ++i) {
    Assert.equal(data.charCodeAt(i), responseBody[i]);
  }
  Services.prefs.setBoolPref("network.http.enforce-framing.http1", enforcePref);
  Services.prefs.setBoolPref(
    "network.http.clear_bogus_content_encoding",
    clearBogusContentEncodingPref
  );
  httpserver.stop(do_test_finished);
}

function run_test() {
  clearBogusContentEncodingPref = Services.prefs.getBoolPref(
    "network.http.clear_bogus_content_encoding"
  );
  Services.prefs.setBoolPref("network.http.clear_bogus_content_encoding", true);
  enforcePref = Services.prefs.getBoolPref(
    "network.http.enforce-framing.http1"
  );
  Services.prefs.setBoolPref("network.http.enforce-framing.http1", false);

  httpserver = new HttpServer();
  httpserver.registerPathHandler("/cached/test.gz", cachedHandler);
  httpserver.start(-1);

  // wipe out cached content
  evict_cache_entries();

  var chan = make_channel(
    "http://localhost:" + httpserver.identity.primaryPort + "/cached/test.gz"
  );
  chan.asyncOpen(
    new ChannelListener(continue_test, null, CL_EXPECT_GZIP | CL_IGNORE_CL)
  );
  do_test_pending();
}
