// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.enabled is working
async function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("xpinstall.enabled", false);

  let tab = BrowserTestUtils.addTab(gBrowser, TESTROOT);
  gBrowser.selectedTab = tab;
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, TESTROOT);

  ContentTask.spawn(
    gBrowser.selectedBrowser,
    TESTROOT + "enabled.html",
    function(url) {
      return new Promise(resolve => {
        function page_loaded() {
          content.removeEventListener("PageLoaded", page_loaded);
          resolve(content.document.getElementById("enabled").textContent);
        }

        function load_listener() {
          removeEventListener("load", load_listener, true);
          content.addEventListener("PageLoaded", page_loaded);
        }

        addEventListener("load", load_listener, true);

        content.location.href = url;
      });
    }
  ).then(text => {
    is(text, "false", "installTrigger should have not been enabled");
    Services.prefs.clearUserPref("xpinstall.enabled");
    gBrowser.removeCurrentTab();
    finish();
  });
}
