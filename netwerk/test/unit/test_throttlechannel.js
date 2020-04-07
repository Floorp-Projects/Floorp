// Test nsIThrottledInputChannel interface.
"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

function test_handler(metadata, response) {
  const originalBody = "the response";
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(originalBody, originalBody.length);
}

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function run_test() {
  let httpserver = new HttpServer();
  httpserver.start(-1);
  const PORT = httpserver.identity.primaryPort;

  httpserver.registerPathHandler("/testdir", test_handler);

  let channel = make_channel("http://localhost:" + PORT + "/testdir");

  let tq = Cc["@mozilla.org/network/throttlequeue;1"].createInstance(
    Ci.nsIInputChannelThrottleQueue
  );
  tq.init(1000, 1000);

  let tic = channel.QueryInterface(Ci.nsIThrottledInputChannel);
  tic.throttleQueue = tq;

  channel.asyncOpen(
    new ChannelListener(() => {
      ok(tq.bytesProcessed() > 0, "throttled queue processed some bytes");

      httpserver.stop(do_test_finished);
    })
  );

  do_test_pending();
}
