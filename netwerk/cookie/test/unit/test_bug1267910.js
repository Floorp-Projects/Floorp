/*
 * Bug 1267910 - Add test cases for the backward compatiability and originAttributes
 *               of nsICookieManager2.
 */

var {utils: Cu, interfaces: Ci, classes: Cc} = Components;

Cu.import("resource://gre/modules/Services.jsm");

const BASE_URL = "http://example.org/";

const COOKIE = {
  host: BASE_URL,
  path: "/",
  name: "test1",
  value: "yes",
  isSecure: false,
  isHttpOnly: false,
  isSession: true,
  expiry: 2145934800,
};

const COOKIE_OA_DEFAULT = {
  host: BASE_URL,
  path: "/",
  name: "test0",
  value: "yes0",
  isSecure: false,
  isHttpOnly: false,
  isSession: true,
  expiry: 2145934800,
  originAttributes: {},
};

const COOKIE_OA_1 = {
  host: BASE_URL,
  path: "/",
  name: "test1",
  value: "yes1",
  isSecure: false,
  isHttpOnly: false,
  isSession: true,
  expiry: 2145934800,
  originAttributes: {userContextId: 1},
};

function checkCookie(cookie, cookieObj) {
  for (let prop of Object.keys(cookieObj)) {
    if (prop === "originAttributes") {
      ok(ChromeUtils.isOriginAttributesEqual(cookie[prop], cookieObj[prop]),
        "Check cookie: " + prop);
    } else {
      equal(cookie[prop], cookieObj[prop], "Check cookie: " + prop);
    }
  }
}

function countCookies(enumerator) {
  let cnt = 0;

  while (enumerator.hasMoreElements()) {
    cnt++;
    enumerator.getNext();
  }

  return cnt;
}

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // Enable user context id
  Services.prefs.setBoolPref("privacy.userContext.enabled", true);

  add_test(test_backward_compatiability);
  add_test(test_originAttributes);


  run_next_test();
}

/*
 * Test for backward compatiablility that APIs works correctly without
 * originAttributes.
 */
function test_backward_compatiability() {
  // Clear cookies.
  Services.cookies.removeAll();

  // Call Add() to add a cookie without originAttributes
  Services.cookies.add(COOKIE.host,
                       COOKIE.path,
                       COOKIE.name,
                       COOKIE.value,
                       COOKIE.isSecure,
                       COOKIE.isHttpOnly,
                       COOKIE.isSession,
                       COOKIE.expiry);

  // Call getCookiesFromHost() to get cookies without originAttributes
  let enumerator = Services.cookies.getCookiesFromHost(BASE_URL);

  ok(enumerator.hasMoreElements(), "Cookies available");
  let foundCookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);

  checkCookie(foundCookie, COOKIE);

  ok(!enumerator.hasMoreElements(), "We should get only one cookie");

  run_next_test();
}

/*
 * Test for originAttributes.
 */
function test_originAttributes() {
  // Clear cookies.
  Services.cookies.removeAll();

  // Add a cookie for default originAttributes.
  Services.cookies.add(COOKIE_OA_DEFAULT.host,
                       COOKIE_OA_DEFAULT.path,
                       COOKIE_OA_DEFAULT.name,
                       COOKIE_OA_DEFAULT.value,
                       COOKIE_OA_DEFAULT.isSecure,
                       COOKIE_OA_DEFAULT.isHttpOnly,
                       COOKIE_OA_DEFAULT.isSession,
                       COOKIE_OA_DEFAULT.expiry,
                       COOKIE_OA_DEFAULT.originAttributes);

  // Get cookies for default originAttributes.
  let enumerator = Services.cookies.getCookiesFromHost(BASE_URL, COOKIE_OA_DEFAULT.originAttributes);

  // Check that do we get cookie correctly.
  ok(enumerator.hasMoreElements(), "Cookies available");
  let foundCookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
  checkCookie(foundCookie, COOKIE_OA_DEFAULT);

  // We should only get one cookie.
  ok(!enumerator.hasMoreElements(), "We should get only one cookie");

  // Get cookies for originAttributes with user context id 1.
  enumerator = Services.cookies.getCookiesFromHost(BASE_URL, COOKIE_OA_1.originAttributes);

  // Check that we will not get cookies if the originAttributes is different.
  ok(!enumerator.hasMoreElements(), "No cookie should be here");

  // Add a cookie for originAttributes with user context id 1.
  Services.cookies.add(COOKIE_OA_1.host,
                       COOKIE_OA_1.path,
                       COOKIE_OA_1.name,
                       COOKIE_OA_1.value,
                       COOKIE_OA_1.isSecure,
                       COOKIE_OA_1.isHttpOnly,
                       COOKIE_OA_1.isSession,
                       COOKIE_OA_1.expiry,
                       COOKIE_OA_1.originAttributes);

  // Get cookies for originAttributes with user context id 1.
  enumerator = Services.cookies.getCookiesFromHost(BASE_URL, COOKIE_OA_1.originAttributes);

  // Check that do we get cookie correctly.
  ok(enumerator.hasMoreElements(), "Cookies available");
  foundCookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
  checkCookie(foundCookie, COOKIE_OA_1);

  // We should only get one cookie.
  ok(!enumerator.hasMoreElements(), "We should get only one cookie");

  // Check that add a cookie will not affect cookies in different originAttributes.
  enumerator = Services.cookies.getCookiesFromHost(BASE_URL, COOKIE_OA_DEFAULT.originAttributes);
  equal(countCookies(enumerator), 1, "We should get only one cookie for default originAttributes");

  // Remove a cookie for originAttributes with user context id 1.
  Services.cookies.remove(COOKIE_OA_1.host, COOKIE_OA_1.name, COOKIE_OA_1.path,
                          false, COOKIE_OA_1.originAttributes);

  // Check that remove will not affect cookies in default originAttributes.
  enumerator = Services.cookies.getCookiesFromHost(BASE_URL, COOKIE_OA_DEFAULT.originAttributes);
  equal(countCookies(enumerator), 1, "Get one cookie for default originAttributes.");

  // Check that should be no cookie for originAttributes with user context id 1.
  enumerator = Services.cookies.getCookiesFromHost(BASE_URL, COOKIE_OA_1.originAttributes);
  equal(countCookies(enumerator), 0, "No cookie shold be here");

  // Remove a cookie for default originAttributes.
  Services.cookies.remove(COOKIE_OA_DEFAULT.host, COOKIE_OA_DEFAULT.name, COOKIE_OA_DEFAULT.path,
                          false, COOKIE_OA_DEFAULT.originAttributes);

  // Check remove() works correctly for default originAttributes.
  enumerator = Services.cookies.getCookiesFromHost(BASE_URL, COOKIE_OA_DEFAULT.originAttributes);
  equal(countCookies(enumerator), 0, "No cookie shold be here");

  run_next_test();
}
