/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the cookie APIs behave sanely after 'profile-before-change'.

"use strict";

var test_generator = do_run_test();

function run_test() {
  do_test_pending();
  test_generator.next();
}

function finish_test() {
  executeSoon(function() {
    test_generator.return();
    do_test_finished();
  });
}

function* do_run_test() {
  // Set up a profile.
  let profile = do_get_profile();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // Start the cookieservice.
  Services.cookies;

  // Set a cookie.
  let uri = NetUtil.newURI("http://foo.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  Services.cookies.setCookieString(uri, "oh=hai; max-age=1000", null);
  let cookies = Services.cookiemgr.cookies;
  Assert.ok(cookies.length == 1);
  let cookie = cookies[0];

  // Fire 'profile-before-change'.
  do_close_profile();

  // Check that the APIs behave appropriately.
  Assert.equal(Services.cookies.getCookieStringForPrincipal(principal), "");
  Assert.equal(Services.cookies.getCookieStringFromHttp(uri, null), "");
  Services.cookies.setCookieString(uri, "oh2=hai", null);
  Services.cookies.setCookieStringFromHttp(uri, "oh3=hai", null);
  Assert.equal(Services.cookies.getCookieStringForPrincipal(principal), "");

  do_check_throws(function() {
    Services.cookiemgr.removeAll();
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.cookies;
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.add(
      "foo.com",
      "",
      "oh4",
      "hai",
      false,
      false,
      false,
      0,
      {},
      Ci.nsICookie.SAMESITE_NONE
    );
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.remove("foo.com", "", "oh4", {});
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    let file = profile.clone();
    file.append("cookies.txt");
    Services.cookiemgr.importCookies(file);
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.cookieExists(cookie.host, cookie.path, cookie.name, {});
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookies.countCookiesFromHost("foo.com");
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookies.getCookiesFromHost("foo.com", {});
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  // Wait for the database to finish closing.
  new _observer(test_generator, "cookie-db-closed");
  yield;

  // Load the profile and check that the API is available.
  do_load_profile();
  Assert.ok(
    Services.cookiemgr.cookieExists(cookie.host, cookie.path, cookie.name, {})
  );

  finish_test();
}
