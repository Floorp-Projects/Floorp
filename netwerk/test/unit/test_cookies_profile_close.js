/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the cookie APIs behave sanely after 'profile-before-change'.

"use strict";

add_task(async () => {
  // Set up a profile.
  do_get_profile();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref("dom.security.https_first", false);

  // Start the cookieservice.
  Services.cookies;

  CookieXPCShellUtils.createServer({ hosts: ["foo.com"] });

  // Set a cookie.
  let uri = NetUtil.newURI("http://foo.com");
  let channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });

  Services.scriptSecurityManager.createContentPrincipal(uri, {});

  await CookieXPCShellUtils.setCookieToDocument(
    uri.spec,
    "oh=hai; max-age=1000"
  );

  let cookies = Services.cookiemgr.cookies;
  Assert.ok(cookies.length == 1);
  let cookie = cookies[0];

  // Fire 'profile-before-change'.
  do_close_profile();

  let promise = new _promise_observer("cookie-db-closed");

  // Check that the APIs behave appropriately.
  Assert.equal(
    await CookieXPCShellUtils.getCookieStringFromDocument("http://foo.com/"),
    ""
  );

  Assert.equal(Services.cookiesvc.getCookieStringFromHttp(uri, channel), "");

  await CookieXPCShellUtils.setCookieToDocument(uri.spec, "oh2=hai");

  Services.cookiesvc.setCookieStringFromHttp(uri, "oh3=hai", channel);
  Assert.equal(
    await CookieXPCShellUtils.getCookieStringFromDocument("http://foo.com/"),
    ""
  );

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
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SCHEME_HTTPS
    );
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.remove("foo.com", "", "oh4", {});
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
  await promise;

  // Load the profile and check that the API is available.
  do_load_profile();
  Assert.ok(
    Services.cookiemgr.cookieExists(cookie.host, cookie.path, cookie.name, {})
  );
  Services.prefs.clearUserPref("dom.security.https_first");
});
