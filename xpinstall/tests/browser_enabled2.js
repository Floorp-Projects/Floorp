// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);
scriptLoader.loadSubScript("chrome://mochikit/content/browser/xpinstall/tests/harness.js", this);

// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.enabled is working
function test() {
  waitForExplicitFinish();

  var prefs = Components.classes["@mozilla.org/preferences;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  prefs.setBoolPref("xpinstall.enabled", false);

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    // Allow the in-page load handler to run first
    executeSoon(page_loaded);
  }, true);
  gBrowser.loadURI(TESTROOT + "enabled.html");
}

function page_loaded() {
  gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, false);
  var prefs = Components.classes["@mozilla.org/preferences;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  prefs.clearUserPref("xpinstall.enabled");

  var doc = gBrowser.contentDocument;
  is(doc.getElementById("enabled").textContent, "false", "installTrigger should have not been enabled");
  gBrowser.removeCurrentTab();
  finish();
}
