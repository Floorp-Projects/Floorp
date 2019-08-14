// Test whether a request for auth for an XPI switches to the appropriate tab
var gNewTab;

function test() {
  // Turn off the authentication dialog blocking for this test.
  Services.prefs.setBoolPref(
    "network.auth.non-web-content-triggered-resources-http-auth-allow",
    true
  );

  Harness.authenticationCallback = get_auth_info;
  Harness.downloadFailedCallback = download_failed;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI":
        TESTROOT + "authRedirect.sjs?" + TESTROOT + "amosigned.xpi",
    })
  );
  gNewTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(
    gBrowser.getBrowserForTab(gNewTab),
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function get_auth_info() {
  is(
    gBrowser.selectedTab,
    gNewTab,
    "Should have focused the tab loading the XPI"
  );
  return ["testuser", "testpass"];
}

function download_failed(install) {
  ok(false, "Install should not have failed");
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 1, "1 Add-on should have been successfully installed");
  var authMgr = Cc["@mozilla.org/network/http-auth-manager;1"].getService(
    Ci.nsIHttpAuthManager
  );
  authMgr.clearAll();

  PermissionTestUtils.remove("http://example.com", "install");

  Services.prefs.clearUserPref(
    "network.auth.non-web-content-triggered-resources-http-auth-allow"
  );

  gBrowser.removeTab(gNewTab);
  Harness.finish();
}
