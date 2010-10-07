// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.enabled is working
function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("xpinstall.enabled", false);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    // Allow the in-page load handler to run first
    executeSoon(page_loaded);
  }, true);
  gBrowser.loadURI(TESTROOT + "enabled.html");
}

function page_loaded() {
  Services.prefs.clearUserPref("xpinstall.enabled");

  var doc = gBrowser.contentDocument;
  is(doc.getElementById("enabled").textContent, "false", "installTrigger should have not been enabled");
  gBrowser.removeCurrentTab();
  finish();
}
