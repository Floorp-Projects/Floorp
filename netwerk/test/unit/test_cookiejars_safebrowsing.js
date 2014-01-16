/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Description of the test:
 *   We show that we can separate the safebrowsing cookie by creating a custom
 *   LoadContext using a reserved AppId (UINT_32_MAX - 1). Setting this
 *   custom LoadContext as a callback on the channel allows us to query the
 *   AppId and therefore separate the safebrowing cookie in its own cookie-jar.
 *   For testing safebrowsing update we do >> NOT << emulate a response
 *   in the body, rather we only set the cookies in the header of the response
 *   and confirm that cookies are separated in their own cookie-jar.
 *
 * 1) We init safebrowsing and simulate an update (cookies are set for localhost)
 *
 * 2) We open a channel that should send regular cookies, but not the
 *    safebrowsing cookie.
 *
 * 3) We open a channel with a custom callback, simulating a safebrowsing cookie
 *    that should send this simulated safebrowsing cookie as well as the
 *    real safebrowsing cookies. (Confirming that the safebrowsing cookies
 *    actually get stored in the correct jar).
 */

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

XPCOMUtils.defineLazyModuleGetter(this, "SafeBrowsing",
  "resource://gre/modules/SafeBrowsing.jsm");

var setCookiePath = "/setcookie";
var checkCookiePath = "/checkcookie";
var safebrowsingUpdatePath = "/safebrowsingUpdate";
var httpserver;

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
           .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function cookieSetHandler(metadata, response) {
  var cookieName = metadata.getHeader("set-cookie");
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("set-Cookie", cookieName + "=1; Path=/", false);
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write("Ok", "Ok".length);
}

function cookieCheckHandler(metadata, response) {
  var cookies = metadata.getHeader("Cookie");
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("saw-cookies", cookies, false);
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write("Ok", "Ok".length);
}

function safebrowsingUpdateHandler(metadata, response) {
  var cookieName = "sb-update-cookie";
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("set-Cookie", cookieName + "=1; Path=/", false);
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write("Ok", "Ok".length);
}

function setupChannel(path, loadContext) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var channel = ios.newChannel(URL + path, "", null);
  channel.notificationCallbacks = loadContext;
  channel.QueryInterface(Ci.nsIHttpChannel);
  return channel;
}

function run_test() {

  // Set up a profile
  do_get_profile();

  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess())
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  httpserver = new HttpServer();
  httpserver.registerPathHandler(setCookiePath, cookieSetHandler);
  httpserver.registerPathHandler(checkCookiePath, cookieCheckHandler);
  httpserver.registerPathHandler(safebrowsingUpdatePath, safebrowsingUpdateHandler);

  httpserver.start(-1);
  run_next_test();
}

// this test does not emulate a response in the body,
// rather we only set the cookies in the header of response.
add_test(function test_safebrowsing_update() {

  var dbservice = Cc["@mozilla.org/url-classifier/dbservice;1"]
                  .getService(Ci.nsIUrlClassifierDBService);
  var streamUpdater = Cc["@mozilla.org/url-classifier/streamupdater;1"]
                     .getService(Ci.nsIUrlClassifierStreamUpdater);

  streamUpdater.updateUrl = URL + safebrowsingUpdatePath;

  function onSuccess() {
    run_next_test();
  }
  function onUpdateError() {
    do_throw("ERROR: received onUpdateError!");
  }
  function onDownloadError() {
    do_throw("ERROR: received onDownloadError!");
  }

  streamUpdater.downloadUpdates("test-phish-simple,test-malware-simple", "",
    onSuccess, onUpdateError, onDownloadError);
});

add_test(function test_non_safebrowsing_cookie() {

  var cookieName = 'regCookie_id0';
  var loadContext = new LoadContextCallback(0, false, false, false);

  function setNonSafeBrowsingCookie() {
    var channel = setupChannel(setCookiePath, loadContext);
    channel.setRequestHeader("set-cookie", cookieName, false);
    channel.asyncOpen(new ChannelListener(checkNonSafeBrowsingCookie, null), null);
  }

  function checkNonSafeBrowsingCookie() {
    var channel = setupChannel(checkCookiePath, loadContext);
    channel.asyncOpen(new ChannelListener(completeCheckNonSafeBrowsingCookie, null), null);
  }

  function completeCheckNonSafeBrowsingCookie(request, data, context) {
    // Confirm that only the >> ONE << cookie is sent over the channel.
    var expectedCookie = cookieName + "=1";
    request.QueryInterface(Ci.nsIHttpChannel);
    var cookiesSeen = request.getResponseHeader("saw-cookies");
    do_check_eq(cookiesSeen, expectedCookie);
    run_next_test();
  }

  setNonSafeBrowsingCookie();
});

add_test(function test_safebrowsing_cookie() {

  var cookieName = 'sbCookie_id4294967294';
  var loadContext = new LoadContextCallback(Ci.nsIScriptSecurityManager.SAFEBROWSING_APP_ID, false, false, false);

  function setSafeBrowsingCookie() {
    var channel = setupChannel(setCookiePath, loadContext);
    channel.setRequestHeader("set-cookie", cookieName, false);
    channel.asyncOpen(new ChannelListener(checkSafeBrowsingCookie, null), null);
  }

  function checkSafeBrowsingCookie() {
    var channel = setupChannel(checkCookiePath, loadContext);
    channel.asyncOpen(new ChannelListener(completeCheckSafeBrowsingCookie, null), null);
  }

  function completeCheckSafeBrowsingCookie(request, data, context) {
    // Confirm that all >> THREE << cookies are sent back over the channel:
    //   a) the safebrowsing cookie set when updating
    //   b) the regular cookie with custom loadcontext defined in this test.
    var expectedCookies = "sb-update-cookie=1; ";
    expectedCookies += cookieName + "=1";
    request.QueryInterface(Ci.nsIHttpChannel);
    var cookiesSeen = request.getResponseHeader("saw-cookies");

    do_check_eq(cookiesSeen, expectedCookies);
    httpserver.stop(do_test_finished);
  }

  setSafeBrowsingCookie();
});
