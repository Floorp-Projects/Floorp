// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.install call fails when xpinstall is disabled
function test() {
  Harness.installDisabledCallback = install_disabled;
  Harness.installBlockedCallback = allow_blocked;
  Harness.installConfirmCallback = confirm_install;
  Harness.setup();

  Services.prefs.setBoolPref("xpinstall.enabled", false);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();

  function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("load", loadListener, true);
    gBrowser.contentWindow.addEventListener("InstallTriggered", page_loaded, false);
  }

  gBrowser.selectedBrowser.addEventListener("load", loadListener, true);
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
  gBrowser.contentWindow.removeEventListener("InstallTriggered", page_loaded, false);
  Services.prefs.clearUserPref("xpinstall.enabled");

  var doc = gBrowser.contentDocument;
  is(doc.getElementById("return").textContent, "false", "installTrigger should have not been enabled");
  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
