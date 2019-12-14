// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.enabled is working
add_task(async function() {
  let testtab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TESTROOT + "bug638292.html"
  );

  async function verify(link, button) {
    info("Clicking " + link);

    let loadedPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#" + link,
      { button },
      gBrowser.selectedBrowser
    );

    let newtab = await loadedPromise;

    let result = await SpecialPowers.spawn(
      newtab.linkedBrowser,
      [],
      async function() {
        return content.document.getElementById("enabled").textContent == "true";
      }
    );

    ok(result, "installTrigger for " + link + " should have been enabled");

    // Focus the old tab (link3 is opened in the background)
    if (link != "link3") {
      await BrowserTestUtils.switchTab(gBrowser, testtab);
    }
    gBrowser.removeTab(newtab);
  }

  await verify("link1", 0);
  await verify("link2", 0);
  await verify("link3", 1);

  gBrowser.removeCurrentTab();
});
