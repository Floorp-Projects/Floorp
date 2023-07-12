"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserver = new HttpServer();

function setupChannel(suffix) {
  return NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + suffix,
    loadUsingSystemPrincipal: true,
  });
}

function checkValueAndTrigger(request, data, ctx) {
  Assert.equal("Ok", data);
  httpserver.stop(do_test_finished);
}

function run_test() {
  // We don't want to have CookieJarSettings blocking this test.
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

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
  if (!metadata.hasHeader("Cookie")) {
    response.setStatusLine(metadata.httpVersion, 302, "Found");
    response.setHeader("Cache-Control", "max-age=600", false);
    response.setHeader("Location", "/redirect2?query", false);
    response.setHeader("Set-Cookie", "MyCookie=1", false);
  } else {
    response.setStatusLine(metadata.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/plain");
    response.bodyOutputStream.write("Ok", "Ok".length);
  }
}

function redirectHandler2(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Location", "/redirect1", false);
}
