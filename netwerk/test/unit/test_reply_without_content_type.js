//
// tests HTTP replies that lack content-type (where we try to sniff content-type).
//

// Note: sets Cc and Ci variables
"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var testpath = "/simple_plainText";
var httpbody = "<html><body>omg hai</body></html>";
var testpathGZip = "/simple_gzip";
//this is compressed httpbody;
var httpbodyGZip = [
  "0x1f",
  "0x8b",
  "0x8",
  "0x0",
  "0x0",
  "0x0",
  "0x0",
  "0x0",
  "0x0",
  "0x3",
  "0xb3",
  "0xc9",
  "0x28",
  "0xc9",
  "0xcd",
  "0xb1",
  "0xb3",
  "0x49",
  "0xca",
  "0x4f",
  "0xa9",
  "0xb4",
  "0xcb",
  "0xcf",
  "0x4d",
  "0x57",
  "0xc8",
  "0x48",
  "0xcc",
  "0xb4",
  "0xd1",
  "0x7",
  "0xf3",
  "0x6c",
  "0xf4",
  "0xc1",
  "0x52",
  "0x0",
  "0x4",
  "0x99",
  "0x79",
  "0x2b",
  "0x21",
  "0x0",
  "0x0",
  "0x0",
];
var buffer = "";

var dbg = 0;
if (dbg) {
  print("============== START ==========");
}

add_test(function test_plainText() {
  if (dbg) {
    print("============== test_plainText: in");
  }
  httpserver.registerPathHandler(testpath, serverHandler_plainText);
  httpserver.start(-1);
  var channel = setupChannel(testpath);
  // ChannelListener defined in head_channels.js
  channel.asyncOpen(new ChannelListener(checkRequest, channel));
  do_test_pending();
  if (dbg) {
    print("============== test_plainText: out");
  }
});

add_test(function test_GZip() {
  if (dbg) {
    print("============== test_GZip: in");
  }
  httpserver.registerPathHandler(testpathGZip, serverHandler_GZip);
  httpserver.start(-1);
  var channel = setupChannel(testpathGZip);
  // ChannelListener defined in head_channels.js
  channel.asyncOpen(new ChannelListener(checkRequest, channel, CL_EXPECT_GZIP));
  do_test_pending();
  if (dbg) {
    print("============== test_GZip: out");
  }
});

function setupChannel(path) {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + path,
    loadUsingSystemPrincipal: true,
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler_plainText(metadata, response) {
  if (dbg) {
    print("============== serverHandler plainText: in");
  }
  //  no content type set
  //  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
  if (dbg) {
    print("============== serverHandler plainText: out");
  }
}

function serverHandler_GZip(metadata, response) {
  if (dbg) {
    print("============== serverHandler GZip: in");
  }
  //  no content type set
  //  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Encoding", "gzip", false);
  var bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream
  );
  bos.setOutputStream(response.bodyOutputStream);
  bos.writeByteArray(httpbodyGZip);
  if (dbg) {
    print("============== serverHandler GZip: out");
  }
}

function checkRequest(request, data, context) {
  if (dbg) {
    print("============== checkRequest: in");
  }
  Assert.equal(data, httpbody);
  Assert.equal(request.QueryInterface(Ci.nsIChannel).contentType, "text/html");
  httpserver.stop(do_test_finished);
  run_next_test();
  if (dbg) {
    print("============== checkRequest: out");
  }
}
