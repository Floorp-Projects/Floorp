// ----------------------------------------------------------------------------
// Test that an install that requires cookies to be sent fails when cookies
// are set and third party cookies are disabled and the request is to a third
// party.
// This verifies bug 462739
function test() {
  Harness.downloadFailedCallback = download_failed;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  Services.cookies.add(
    "example.org",
    "/browser/" + RELATIVE_DIR,
    "xpinstall",
    "true",
    false,
    false,
    true,
    Date.now() / 1000 + 60,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTP
  );

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Cookie check":
        TESTROOT2 + "cookieRedirect.sjs?" + TESTROOT + "amosigned.xpi",
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function download_failed(install) {
  is(install.error, AddonManager.ERROR_NETWORK_FAILURE, "Install should fail");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");

  Services.cookies.remove(
    "example.org",
    "xpinstall",
    "/browser/" + RELATIVE_DIR,
    {}
  );

  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  PermissionTestUtils.remove("http://example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
