//
// Verify that we hit the net if we discover a cycle of redirects
// coming from cache.
//

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var iteration = 0;

function setupChannel(suffix) {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + suffix,
    loadUsingSystemPrincipal: true,
  });
  var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
  httpChan.requestMethod = "GET";
  return httpChan;
}

function checkValueAndTrigger(request, data, ctx) {
  Assert.equal("Ok", data);
  httpserver.stop(do_test_finished);
}

function run_test() {
  httpserver.registerPathHandler("/redirect1", redirectHandler1);
  httpserver.registerPathHandler("/redirect2", redirectHandler2);
  httpserver.start(-1);

  // clear cache
  evict_cache_entries();

  // load first time
  var channel = setupChannel("/redirect1");
  channel.asyncOpen(new ChannelListener(checkValueAndTrigger, null));

  do_test_pending();
}

function redirectHandler1(metadata, response) {
  // first time we return a cacheable 302 pointing to next redirect
  if (iteration < 1) {
    response.setStatusLine(metadata.httpVersion, 302, "Found");
    response.setHeader("Cache-Control", "max-age=600", false);
    response.setHeader("Location", "/redirect2", false);

    // next time called we return 200
  } else {
    response.setStatusLine(metadata.httpVersion, 200, "Ok");
    response.setHeader("Cache-Control", "max-age=600", false);
    response.setHeader("Content-Type", "text/plain");
    response.bodyOutputStream.write("Ok", "Ok".length);
  }
  iteration += 1;
}

function redirectHandler2(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Cache-Control", "max-age=600", false);
  response.setHeader("Location", "/redirect1", false);
}
