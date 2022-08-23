/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = null;
const noRedirectURI = "/content";
const acceptType = "application/json";

function redirectHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 302, "Moved Temporarily");
  response.setHeader("Location", noRedirectURI, false);
}

function contentHandler(metadata, response) {
  Assert.equal(metadata.getHeader("Accept"), acceptType);
  httpserver.stop(do_test_finished);
}

function dummyHandler(request, buffer) {}

function run_test() {
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/redirect", redirectHandler);
  httpserver.registerPathHandler("/content", contentHandler);
  httpserver.start(-1);

  Services.prefs.setBoolPref("network.http.prompt-temp-redirect", false);

  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + "/redirect",
    loadUsingSystemPrincipal: true,
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.setRequestHeader("Accept", acceptType, false);

  chan.asyncOpen(new ChannelListener(dummyHandler, null));

  do_test_pending();
}
