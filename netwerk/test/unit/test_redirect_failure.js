"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

/*
 * The test is checking async redirect code path that is loading a
 * redirect.  But creation of the target channel fails before we even try
 * to do async open on it. We force the creation error by forbidding
 * the port number the URI contains.
 */

function inChildProcess() {
  return (
    Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
      .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
  );
}

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;
// Need to randomize, because apparently no one clears our cache
var randomPath = "/redirect/" + Math.random();

XPCOMUtils.defineLazyGetter(this, "randomURI", function() {
  return URL + randomPath;
});

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

function redirectHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.setHeader("Location", "http://non-existent.tld:65400", false);
  response.setHeader("Cache-Control", "no-cache", false);
}

function finish_test(request, buffer) {
  Assert.equal(request.status, Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED);

  Assert.equal(buffer, "");
  httpServer.stop(do_test_finished);
}

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler(randomPath, redirectHandler);
  httpServer.start(-1);

  if (!inChildProcess()) {
    Services.prefs.setCharPref("network.security.ports.banned", "65400");
  }

  var chan = make_channel(randomURI);
  chan.asyncOpen(new ChannelListener(finish_test, null, CL_EXPECT_FAILURE));
  do_test_pending();
}
