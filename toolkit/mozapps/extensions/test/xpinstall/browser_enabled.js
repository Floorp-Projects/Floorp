// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.enabled is working
add_task(async function test() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT);

  let text = await ContentTask.spawn(
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
  );

  is(text, "true", "installTrigger should have been enabled");
  gBrowser.removeCurrentTab();
});
