/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
"use strict";
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var httpserver = null;
const noRedirectURI = "/content";
const pageValue = "Final page";
const acceptType = "application/json";

function redirectHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 302, "Moved Temporarily");
  response.setHeader("Location", noRedirectURI, false);
}

function contentHandler(metadata, response)
{
  Assert.equal(metadata.getHeader("Accept"), acceptType);
  httpserver.stop(do_test_finished);
}

function dummyHandler(request, buffer)
{
}

function run_test()
{
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/redirect", redirectHandler);
  httpserver.registerPathHandler("/content", contentHandler);
  httpserver.start(-1);

  var prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);
  prefs.setBoolPref("network.http.prompt-temp-redirect", false);

  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + "/redirect",
    loadUsingSystemPrincipal: true
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.setRequestHeader("Accept", acceptType, false);

  chan.asyncOpen2(new ChannelListener(dummyHandler, null));

  do_test_pending();
}
