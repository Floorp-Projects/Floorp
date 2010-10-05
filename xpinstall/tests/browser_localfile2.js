// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Test whether an install fails if the url is a local file when requested from
// web content
function test() {
  waitForExplicitFinish();

  var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Components.interfaces.nsIChromeRegistry);
  var path = cr.convertChromeURL(makeURI(CHROMEROOT + "unsigned.xpi")).spec;

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": path
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    // Allow the in-page load handler to run first
    executeSoon(page_loaded);
  }, true);
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function page_loaded() {
  var doc = gBrowser.contentDocument;
  is(doc.getElementById("return").textContent, "exception", "installTrigger should have failed");
  gBrowser.removeCurrentTab();
  finish();
}
// ----------------------------------------------------------------------------
