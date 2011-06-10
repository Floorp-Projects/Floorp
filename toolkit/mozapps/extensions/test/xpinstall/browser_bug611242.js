// ----------------------------------------------------------------------------
// Test whether setting a new property in InstallTrigger then persists to other
// page loads
function loadURI(aUri, aCallback) {
  gBrowser.selectedBrowser.addEventListener("load", function() {
    if (gBrowser.selectedBrowser.currentURI.spec != aUri)
      return;

    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    aCallback();
  }, true);

  gBrowser.loadURI(aUri);
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();

  loadURI(TESTROOT + "enabled.html", function() {
    window.content.wrappedJSObject.InstallTrigger.enabled.k = function() { };

    loadURI(TESTROOT2 + "enabled.html", function() {
      is(window.content.wrappedJSObject.InstallTrigger.enabled.k, undefined, "Property should not be defined");

      gBrowser.removeTab(gBrowser.selectedTab);

      finish();
    });
  });
}
// ----------------------------------------------------------------------------
