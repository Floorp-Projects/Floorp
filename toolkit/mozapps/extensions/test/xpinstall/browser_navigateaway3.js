// ----------------------------------------------------------------------------
// Tests that navigating to a new origin cancels ongoing installs.

// Block the modal install UI from showing.
Services.prefs.setBoolPref(PREF_CUSTOM_CONFIRMATION_UI, true);

function test() {
  Harness.downloadProgressCallback = download_progress;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function download_progress(addon, value, maxValue) {
  gBrowser.loadURI(TESTROOT2 + "enabled.html");
}

function install_ended(install, addon) {
  ok(false, "Should not have seen installs complete");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been successfully installed");

  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
