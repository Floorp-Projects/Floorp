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

  function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("load", loadListener, true);
    gBrowser.contentWindow.addEventListener("InstallTriggered", page_loaded, false);
  }

  gBrowser.selectedBrowser.addEventListener("load", loadListener, true);

  // In non-e10s the exception in the content page would trigger a test failure
  if (!gMultiProcessBrowser)
    expectUncaughtException();

  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function page_loaded() {
  gBrowser.contentWindow.removeEventListener("InstallTriggered", page_loaded, false);
  var doc = gBrowser.contentDocument;
  is(doc.getElementById("return").textContent, "exception", "installTrigger should have failed");

  // In non-e10s the exception from the page is thrown after the event so we
  // have to spin the event loop to make sure it arrives so expectUncaughtException
  // sees it.
  executeSoon(() => {
    gBrowser.removeCurrentTab();
    finish();
  });
}
// ----------------------------------------------------------------------------
