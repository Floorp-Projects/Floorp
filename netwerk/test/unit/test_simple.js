//
//  Simple HTTP test: fetches page
//

// Note: sets Cc and Ci variables
"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";

var dbg = 0;
if (dbg) {
  print("============== START ==========");
}

function run_test() {
  setup_test();
  do_test_pending();
}

function setup_test() {
  if (dbg) {
    print("============== setup_test: in");
  }
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);
  var channel = setupChannel(testpath);
  // ChannelListener defined in head_channels.js
  channel.asyncOpen(new ChannelListener(checkRequest, channel));
  if (dbg) {
    print("============== setup_test: out");
  }
}

function setupChannel(path) {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + path,
    loadUsingSystemPrincipal: true,
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  if (dbg) {
    print("============== serverHandler: in");
  }
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
  if (dbg) {
    print("============== serverHandler: out");
  }
}

function checkRequest(request, data, context) {
  if (dbg) {
    print("============== checkRequest: in");
  }
  Assert.equal(data, httpbody);
  httpserver.stop(do_test_finished);
  if (dbg) {
    print("============== checkRequest: out");
  }
}
