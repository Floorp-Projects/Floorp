// ----------------------------------------------------------------------------
// Tests installing a signed add-on that has been tampered with after signing.
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.downloadFailedCallback = download_failed;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Tampered Signed XPI": TESTROOT + "signed-tampered.xpi"
  }));
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  ok(false, "Should not offer to install");
}

function download_failed(install) {
  is(install.error, AddonManager.ERROR_CORRUPT_FILE, "Install should fail");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
