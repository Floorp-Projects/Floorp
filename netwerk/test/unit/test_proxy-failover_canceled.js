"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpServer = null;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  });
}

const responseBody = "response body";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function finish_test(request, buffer) {
  Assert.equal(buffer, "");
  httpServer.stop(do_test_finished);
}

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  // we want to cancel the failover proxy engage, so, do not allow
  // redirects from now.

  var nc = new ChannelEventSink();
  nc._flags = ES_ABORT_REDIRECT;

  var prefserv = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefService
  );
  var prefs = prefserv.getBranch("network.proxy.");
  prefs.setIntPref("type", 2);
  prefs.setCharPref("no_proxies_on", "nothing");
  prefs.setBoolPref("allow_hijacking_localhost", true);
  prefs.setCharPref(
    "autoconfig_url",
    "data:text/plain," +
      "function FindProxyForURL(url, host) {return 'PROXY a_non_existent_domain_x7x6c572v:80; PROXY localhost:" +
      httpServer.identity.primaryPort +
      "';}"
  );

  var chan = make_channel(
    "http://localhost:" + httpServer.identity.primaryPort + "/content"
  );
  chan.notificationCallbacks = nc;
  chan.asyncOpen(new ChannelListener(finish_test, null, CL_EXPECT_FAILURE));
  do_test_pending();
}
