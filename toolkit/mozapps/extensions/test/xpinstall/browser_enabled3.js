// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.install call fails when xpinstall is disabled
function test() {
  ignoreAllUncaughtExceptions();
  Harness.installDisabledCallback = install_disabled;
  Harness.installBlockedCallback = allow_blocked;
  Harness.installConfirmCallback = confirm_install;
  Harness.setup();

  Services.prefs.setBoolPref("xpinstall.enabled", false);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    // Allow the in-page load handler to run first
    executeSoon(page_loaded);
  }, true);
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function install_disabled(installInfo) {
  ok(true, "Saw installation disabled");
}

function allow_blocked(installInfo) {
  ok(false, "Should never see the blocked install notification");
  return false;
}

function confirm_install(window) {
  ok(false, "Should never see an install confirmation dialog");
  return false;
}

function page_loaded() {
  Services.prefs.clearUserPref("xpinstall.enabled");

  var doc = gBrowser.contentDocument;
  is(doc.getElementById("return").textContent, "false", "installTrigger should have not been enabled");
  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
