/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const info = console.log;

var EXPORTED_SYMBOLS = [
  "HTTPS_EXAMPLE_ORG",
  "HTTPS_EXAMPLE_COM",
  "HTTP_EXAMPLE_COM",
  "browserTestPath",
  "waitForAllExpectedTests",
  "cleanupObservers",
  "triggerSetCookieFromHttp",
  "triggerSetCookieFromHttpPrivate",
  "checkExpectedCookies",
  "fetchHelper",
  "preclean_test",
  "cleanup_test",
];
var HTTPS_EXAMPLE_ORG = "https://example.org";
var HTTPS_EXAMPLE_COM = "https://example.com";
var HTTP_EXAMPLE_COM = "http://example.com";

function browserTestPath(uri) {
  return uri + "/browser/netwerk/test/browser/";
}

function waitForAllExpectedTests() {
  return ContentTaskUtils.waitForCondition(() => {
    return content.testDone === true;
  });
}

function cleanupObservers() {
  Services.obs.notifyObservers(null, "cookie-content-filter-cleanup");
}

async function preclean_test() {
  // enable all cookies for the set-cookie trigger via setCookieStringFromHttp
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);
  Services.prefs.setBoolPref(
    "network.cookie.sameSite.noneRequiresSecure",
    false
  );
  Services.prefs.setBoolPref("network.cookie.sameSite.schemeful", false);

  Services.cookies.removeAll();
}

async function cleanup_test() {
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref(
    "network.cookieJarSettings.unblocked_for_testing"
  );

  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
  Services.prefs.clearUserPref("network.cookie.sameSite.noneRequiresSecure");
  Services.prefs.clearUserPref("network.cookie.sameSite.schemeful");

  Services.cookies.removeAll();
}

async function fetchHelper(url, cookie, secure, domain = "") {
  let headers = new Headers();

  headers.append("return-set-cookie", cookie);

  if (!secure) {
    headers.append("return-insecure-cookie", cookie);
  }

  if (domain != "") {
    headers.append("return-cookie-domain", domain);
  }

  info("fetching " + url);
  await fetch(url, { headers });
}

// cookie header strings with multiple name=value pairs delimited by \n
// will trigger multiple "cookie-changed" signals
function triggerSetCookieFromHttp(uri, cookie, fpd = "", ucd = 0) {
  info("about to trigger set-cookie: " + uri + " " + cookie);
  let channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });

  if (fpd != "") {
    channel.loadInfo.originAttributes = { firstPartyDomain: fpd };
  }

  if (ucd != 0) {
    channel.loadInfo.originAttributes = { userContextId: ucd };
  }
  Services.cookies.setCookieStringFromHttp(uri, cookie, channel);
}

async function triggerSetCookieFromHttpPrivate(uri, cookie) {
  info("about to trigger set-cookie: " + uri + " " + cookie);
  let channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIPrivateBrowsingChannel);
  channel.loadInfo.originAttributes = { privateBrowsingId: 1 };
  channel.setPrivate(true);
  Services.cookies.setCookieStringFromHttp(uri, cookie, channel);
}

// observer/listener function that will be run on the content processes
// listens and checks for the expected cookies
function checkExpectedCookies(expected, browserName) {
  const COOKIE_FILTER_TEST_MESSAGE = "cookie-content-filter-test";
  const COOKIE_FILTER_TEST_CLEANUP = "cookie-content-filter-cleanup";

  // Counting the expected number of tests is vital to the integrity of these
  // tests due to the fact that this test suite relies on triggering tests
  // to occur on multiple content processes.
  // As such, test modifications/bugs often lead to silent failures.
  // Hence, we count to ensure we didn't break anything
  // To reduce risk here, we modularize each test as much as possible to
  // increase liklihood that a silent failure will trigger a no-test
  // error/warning
  content.testDone = false;
  let testNumber = 0;

  // setup observer that continues listening/testing
  function obs(subject, topic) {
    // cleanup trigger recieved -> tear down the observer
    if (topic == COOKIE_FILTER_TEST_CLEANUP) {
      info("cleaning up: " + browserName);
      Services.obs.removeObserver(obs, COOKIE_FILTER_TEST_MESSAGE);
      Services.obs.removeObserver(obs, COOKIE_FILTER_TEST_CLEANUP);
      return;
    }

    // test trigger recv'd -> perform test on cookie contents
    if (topic == COOKIE_FILTER_TEST_MESSAGE) {
      info("Checking if cookie visible: " + browserName);
      let result = content.document.cookie;
      let resultStr =
        "Result " +
        result +
        " == expected: " +
        expected[testNumber] +
        " in " +
        browserName;
      ok(result == expected[testNumber], resultStr);
      testNumber++;
      if (testNumber >= expected.length) {
        info("finishing browser tests: " + browserName);
        content.testDone = true;
      }
      return;
    }

    ok(false, "Didn't handle cookie message properly"); //
  }

  info("setting up observers: " + browserName);
  Services.obs.addObserver(obs, COOKIE_FILTER_TEST_MESSAGE);
  Services.obs.addObserver(obs, COOKIE_FILTER_TEST_CLEANUP);
}
