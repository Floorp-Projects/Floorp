/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
"use strict";
Cu.import("resource://testing-common/httpd.js");

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
  do_check_eq(metadata.getHeader("Accept"), acceptType);
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

  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:" +
                            httpserver.identity.primaryPort + "/redirect",
                            "",
                            null);

  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.setRequestHeader("Accept", acceptType, false);

  chan.asyncOpen(new ChannelListener(dummyHandler, null), null);

  do_test_pending();
}
