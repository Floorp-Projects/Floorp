"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
/*
 * The test is checking async redirect code path that is loading a cached
 * redirect.  But creation of the target channel fails before we even try
 * to do async open on it. We force the creation error by forbidding
 * the port number the URI contains. It must be done only after we have
 * attempted to do the redirect (open the target URL) otherwise it's not
 * cached.
 */

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;
// Need to randomize, because apparently no one clears our cache
var randomPath = "/redirect/" + Math.random();

ChromeUtils.defineLazyGetter(this, "randomURI", function () {
  return URL + randomPath;
});

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

var serverRequestCount = 0;

function redirectHandler(metadata, response) {
  ++serverRequestCount;
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.setHeader("Location", "http://non-existent.tld:65400", false);
  response.setHeader("Cache-control", "max-age=1000", false);
}

function firstTimeThrough(request) {
  Assert.equal(request.status, Cr.NS_ERROR_UNKNOWN_HOST);
  Assert.equal(serverRequestCount, 1);

  const nextHop = () => {
    var chan = make_channel(randomURI);
    chan.loadFlags |= Ci.nsIRequest.LOAD_FROM_CACHE;
    chan.asyncOpen(new ChannelListener(finish_test, null, CL_EXPECT_FAILURE));
  };

  if (inChildProcess()) {
    do_send_remote_message("disable-ports");
    do_await_remote_message("disable-ports-done").then(nextHop);
  } else {
    Services.prefs.setCharPref("network.security.ports.banned", "65400");
    nextHop();
  }
}

function finish_test(request, buffer) {
  Assert.equal(request.status, Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED);
  Assert.equal(serverRequestCount, 1);
  Assert.equal(buffer, "");

  httpServer.stop(do_test_finished);
}

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler(randomPath, redirectHandler);
  httpServer.start(-1);

  var chan = make_channel(randomURI);
  chan.asyncOpen(
    new ChannelListener(firstTimeThrough, null, CL_EXPECT_FAILURE)
  );
  do_test_pending();
}
