// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Tests that going offline cancels an in progress download.
function test() {
  Harness.downloadProgressCallback = download_progress;
  Harness.installEndedCallback = check_xpi_install;
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

function check_xpi_install(addon, status) {
  is(status, -210, "Install should be cancelled");
}

function finish_test() {
  try {
    Services.prefs.setBoolPref("browser.offline", false);
    Services.io.offline = false;
  } catch (ex) {
  }

  Services.perms.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
