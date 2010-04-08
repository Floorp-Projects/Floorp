// ----------------------------------------------------------------------------
// Tests that going offline cancels an in progress download.
function test() {
  Harness.downloadProgressCallback = download_progress;
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
  try {
    Services.io.manageOfflineStatus = false;
    Services.prefs.setBoolPref("browser.offline", true);
    Services.io.offline = true;
  } catch (ex) {
  }
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been installed");
  try {
    Services.prefs.setBoolPref("browser.offline", false);
    Services.io.offline = false;
  } catch (ex) {
  }

  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
