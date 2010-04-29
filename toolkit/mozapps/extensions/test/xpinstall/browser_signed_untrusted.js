// ----------------------------------------------------------------------------
// Tests installing an add-on signed by an untrusted certificate through an
// InstallTrigger call in web content.
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = download_failed;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Untrusted Signed XPI": TESTROOT + "signed-untrusted.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function confirm_install(window) {
  ok(false, "Should not offer to install");
}

function download_failed(install, status) {
  is(status, AddonManager.ERROR_CORRUPTFILE, "Should have seen a corrupt file");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
