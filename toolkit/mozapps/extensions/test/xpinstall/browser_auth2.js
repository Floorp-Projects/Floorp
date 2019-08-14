// ----------------------------------------------------------------------------
// Test whether an install fails when authentication is required and bad
// credentials are given
// This verifies bug 312473
function test() {
  // Turn off the authentication dialog blocking for this test.
  Services.prefs.setBoolPref(
    "network.auth.non-web-content-triggered-resources-http-auth-allow",
    true
  );

  requestLongerTimeout(2);
  Harness.authenticationCallback = get_auth_info;
  Harness.downloadFailedCallback = download_failed;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI":
        TESTROOT + "authRedirect.sjs?" + TESTROOT + "amosigned.xpi",
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function get_auth_info() {
  return ["baduser", "badpass"];
}

function download_failed(install) {
  is(
    install.error,
    AddonManager.ERROR_NETWORK_FAILURE,
    "Install should have failed"
  );
}

function install_ended(install, addon) {
  ok(false, "Add-on should not have installed");
  install.cancel();
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  var authMgr = Cc["@mozilla.org/network/http-auth-manager;1"].getService(
    Ci.nsIHttpAuthManager
  );
  authMgr.clearAll();

  Services.perms.remove(makeURI("http://example.com"), "install");

  Services.prefs.clearUserPref(
    "network.auth.non-web-content-triggered-resources-http-auth-allow"
  );

  gBrowser.removeCurrentTab();
  Harness.finish();
}
