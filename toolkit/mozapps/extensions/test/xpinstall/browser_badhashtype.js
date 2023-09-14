// ----------------------------------------------------------------------------
// Test whether an install fails when an unknown hash type is included
// This verifies bug 302284
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
      "Unsigned XPI": {
        URL: TESTROOT + "amosigned.xpi",
        Hash: "foo:3d0dc22e1f394e159b08aaf5f0f97de4d5c65f4f",
        toString() {
          return this.URL;
        },
      },
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );
}

function download_failed(install) {
  is(install.error, AddonManager.ERROR_INCORRECT_HASH, "Install should fail");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  PermissionTestUtils.remove("http://example.com/", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
