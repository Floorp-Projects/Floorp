// ----------------------------------------------------------------------------
// Test that an install that requires cookies to be sent fails when no cookies
// are set
// This verifies bug 462739
function test() {
  // This test depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  Harness.downloadFailedCallback = download_failed;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Cookie check":
        TESTROOT + "cookieRedirect.sjs?" + TESTROOT + "amosigned.xpi",
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function download_failed(install) {
  is(install.error, AddonManager.ERROR_NETWORK_FAILURE, "Install should fail");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  PermissionTestUtils.remove("http://example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
