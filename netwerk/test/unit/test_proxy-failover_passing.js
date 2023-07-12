"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpServer = null;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

const responseBody = "response body";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function finish_test(request, buffer) {
  Assert.equal(buffer, responseBody);
  httpServer.stop(do_test_finished);
}

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  var prefs = Services.prefs.getBranch("network.proxy.");
  prefs.setIntPref("type", 2);
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
  chan.asyncOpen(new ChannelListener(finish_test, null));
  do_test_pending();
}
