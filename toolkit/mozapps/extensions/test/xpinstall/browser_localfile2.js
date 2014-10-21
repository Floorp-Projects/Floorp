// ----------------------------------------------------------------------------
// Test whether an install fails if the url is a local file when requested from
// web content
function test() {
  waitForExplicitFinish();

  var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Components.interfaces.nsIChromeRegistry);
  
  var chromeroot = getChromeRoot(gTestPath);              
  try {
    var xpipath = cr.convertChromeURL(makeURI(chromeroot + "unsigned.xpi")).spec;
  } catch (ex) {
    var xpipath = chromeroot + "unsigned.xpi"; //scenario where we are running from a .jar and already extracted
  }
  
  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": xpipath
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    // Allow the in-page load handler to run first
    executeSoon(page_loaded);
  }, true);

  // In non-e10s the exception in the content page would trigger a test failure
  if (!gMultiProcessBrowser)
    expectUncaughtException();

  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function page_loaded() {
  var doc = gBrowser.contentDocument;
  is(doc.getElementById("return").textContent, "exception", "installTrigger should have failed");
  gBrowser.removeCurrentTab();
  finish();
}
// ----------------------------------------------------------------------------
