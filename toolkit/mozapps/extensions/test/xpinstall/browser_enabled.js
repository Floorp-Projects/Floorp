// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.enabled is working
function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TESTROOT);

  SpecialPowers.spawn(
    gBrowser.selectedBrowser, [TESTROOT + "enabled.html"], function(url) {
      return new Promise(resolve => {
        function page_loaded() {
          content.removeEventListener("PageLoaded", page_loaded);
          resolve(content.document.getElementById("enabled").textContent);
        }

        function load_listener() {
          docShell.chromeEventHandler.removeEventListener("load", load_listener, true);
          content.addEventListener("PageLoaded", page_loaded);
        }

        docShell.chromeEventHandler.addEventListener("load", load_listener, true);

        content.location.href = url;
      });
    }
  ).then(text => {
    is(text, "true", "installTrigger should have been enabled");
    gBrowser.removeCurrentTab();
    finish();
  });
}
