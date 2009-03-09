// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);
scriptLoader.loadSubScript("chrome://mochikit/content/browser/xpinstall/tests/harness.js", this);

// ----------------------------------------------------------------------------
// Tests installing an unsigned add-on through an installChrome call in web
// content. This should be blocked by the whitelist check.
// This verifies bug 252830
function test() {
  Harness.installBlockedCallback = allow_blocked;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installchrome.html? " + encodeURIComponent(TESTROOT + "unsigned.xpi"));
}

function allow_blocked() {
  return false;
}

function finish_test() {
  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
