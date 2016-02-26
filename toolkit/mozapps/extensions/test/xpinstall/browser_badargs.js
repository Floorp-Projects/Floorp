// ----------------------------------------------------------------------------
// Test whether passing a simple string to InstallTrigger.install throws an
// exception
function test() {
  waitForExplicitFinish();

  var triggers = encodeURIComponent(JSON.stringify(TESTROOT + "amosigned.xpi"));
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
