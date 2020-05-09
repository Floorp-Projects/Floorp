/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the cookie APIs behave sanely after 'profile-before-change'.

"use strict";

add_task(async () => {
  // Set up a profile.
  let profile = do_get_profile();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

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

  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  let contentPage = await CookieXPCShellUtils.loadContentPage(uri.spec);
  await contentPage.spawn(
    null,
    // eslint-disable-next-line no-undef
    () => (content.document.cookie = "oh=hai; max-age=1000")
  );
  await contentPage.close();

  let cookies = Services.cookiemgr.cookies;
  Assert.ok(cookies.length == 1);
  let cookie = cookies[0];

  // Fire 'profile-before-change'.
  do_close_profile();

  let promise = new _promise_observer("cookie-db-closed");

  // Check that the APIs behave appropriately.
  Assert.equal(Services.cookies.getCookieStringForPrincipal(principal), "");
  Assert.equal(Services.cookies.getCookieStringFromHttp(uri, channel), "");

  contentPage = await CookieXPCShellUtils.loadContentPage(uri.spec);
  // eslint-disable-next-line no-undef
  await contentPage.spawn(null, () => (content.document.cookie = "oh2=hai"));
  await contentPage.close();

  Services.cookies.setCookieStringFromHttp(uri, "oh3=hai", channel);
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
});
