"use strict";

do_get_profile();

// This test checks that active private-browsing HTTP channels, do not save
// cookies after the termination of the private-browsing session.

// This test consists in following steps:
// - starts a http server
// - no cookies at this point
// - does a beacon request in private-browsing mode
// - after the completion of the request, a cookie should be set (cookie cleanup)
// - does a beacon request in private-browsing mode and dispatch a
//   last-pb-context-exit notification
// - after the completion of the request, no cookies should be set

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

let server;

function setupServer() {
  info("Starting the server...");

  function beaconHandler(metadata, response) {
    response.setHeader("Cache-Control", "max-age=10000", false);
    response.setStatusLine(metadata.httpVersion, 204, "No Content");
    response.setHeader("Set-Cookie", "a=b; path=/beacon; sameSite=lax", false);
    response.bodyOutputStream.write("", 0);
  }

  server = new HttpServer();
  server.registerPathHandler("/beacon", beaconHandler);
  server.start(-1);
  next();
}

function shutdownServer() {
  info("Terminating the server...");
  server.stop(next);
}

function sendRequest(notification) {
  info("Sending a request...");

  var privateLoadContext = Cu.createPrivateLoadContext();

  var path =
    "http://localhost:" +
    server.identity.primaryPort +
    "/beacon?" +
    Math.random();

  var uri = NetUtil.newURI(path);
  var securityFlags =
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL |
    Ci.nsILoadInfo.SEC_COOKIES_INCLUDE;
  var principal = Services.scriptSecurityManager.createContentPrincipal(uri, {
    privateBrowsingId: 1,
  });

  var chan = NetUtil.newChannel({
    uri,
    loadingPrincipal: principal,
    securityFlags,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_BEACON,
  });

  chan.notificationCallbacks = Cu.createPrivateLoadContext();

  let loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(
    Ci.nsILoadGroup
  );

  loadGroup.notificationCallbacks = Cu.createPrivateLoadContext();
  chan.loadGroup = loadGroup;

  chan.notificationCallbacks = privateLoadContext;
  var channelListener = new ChannelListener(next, null, CL_ALLOW_UNKNOWN_CL);

  if (notification) {
    info("Sending notification...");
    Services.obs.notifyObservers(null, "last-pb-context-exited");
  }

  chan.asyncOpen(channelListener);
}

function checkCookies(hasCookie) {
  let cm = Services.cookies;
  Assert.equal(
    cm.cookieExists("localhost", "/beacon", "a", { privateBrowsingId: 1 }),
    hasCookie
  );
  cm.removeAll();
  next();
}

const steps = [
  setupServer,

  // no cookie at startup
  () => checkCookies(false),

  // no last-pb-context-exit notification
  () => sendRequest(false),
  () => checkCookies(true),

  // last-pb-context-exit notification
  () => sendRequest(true),
  () => checkCookies(false),

  shutdownServer,
];

function next() {
  if (!steps.length) {
    do_test_finished();
    return;
  }

  steps.shift()();
}

function run_test() {
  // We don't want to have CookieJarSettings blocking this test.
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  do_test_pending();
  next();
}
