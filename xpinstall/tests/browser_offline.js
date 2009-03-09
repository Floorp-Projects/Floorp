// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);
scriptLoader.loadSubScript("chrome://mochikit/content/browser/xpinstall/tests/harness.js", this);

// ----------------------------------------------------------------------------
// Tests that going offline cancels an in progress download.
function test() {
  Harness.downloadProgressCallback = download_progress;
  Harness.installEndedCallback = check_xpi_install;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                     .getService(Components.interfaces.nsIPermissionManager);
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function download_progress(addon, value, maxValue) {
  var prefs = Components.classes["@mozilla.org/preferences;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService2);

  try {
    ioService.manageOfflineStatus = false;
    prefs.setBoolPref("browser.offline", true);
    ioService.offline = true;
  } catch (ex) {
  }
}

function check_xpi_install(addon, status) {
  is(status, -210, "Install should be cancelled");
}

function finish_test() {
  var prefs = Components.classes["@mozilla.org/preferences;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService2);
  try {
    prefs.setBoolPref("browser.offline", false);
    ioService.offline = false;
  } catch (ex) {
  }

  var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                     .getService(Components.interfaces.nsIPermissionManager);
  pm.remove("example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
