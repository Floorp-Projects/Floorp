// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.enabled is working
function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("xpinstall.enabled", false);

  gBrowser.selectedTab = gBrowser.addTab();

  function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("load", loadListener, true);
    gBrowser.contentWindow.addEventListener("PageLoaded", page_loaded, false);
  }

  gBrowser.selectedBrowser.addEventListener("load", loadListener, true);
  gBrowser.loadURI(TESTROOT + "enabled.html");
}

function page_loaded() {
  gBrowser.contentWindow.removeEventListener("PageLoaded", page_loaded, false);
  Services.prefs.clearUserPref("xpinstall.enabled");

  var doc = gBrowser.contentDocument;
  is(doc.getElementById("enabled").textContent, "false", "installTrigger should have not been enabled");
  gBrowser.removeCurrentTab();
  finish();
}
