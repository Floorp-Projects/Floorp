// ----------------------------------------------------------------------------
// Test whether an install fails when a invalid hash is included in the HTTPS
// request
// This verifies bug 591070
function test() {
  // This test currently depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  Harness.downloadFailedCallback = download_failed;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);

  var url = "https://example.com/browser/" + RELATIVE_DIR + "hashRedirect.sjs";
  url += "?sha1:foobar|" + TESTROOT + "amosigned.xpi";

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": {
        URL: url,
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
  is(install.error, AddonManager.ERROR_INCORRECT_HASH, "Download should fail");
}

function finish_test(count) {
  is(count, 0, "0 Add-ons should have been successfully installed");

  PermissionTestUtils.remove("http://example.com", "install");
  Services.prefs.clearUserPref(PREF_INSTALL_REQUIREBUILTINCERTS);

  gBrowser.removeCurrentTab();
  Harness.finish();
}
