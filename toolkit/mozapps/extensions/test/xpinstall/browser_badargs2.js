// ----------------------------------------------------------------------------
// Test whether passing an undefined url InstallTrigger.install throws an
// exception
function test() {
  waitForExplicitFinish();

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": {
      URL: undefined
    }
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
