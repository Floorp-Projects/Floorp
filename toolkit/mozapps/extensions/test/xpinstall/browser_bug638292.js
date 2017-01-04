// ----------------------------------------------------------------------------
// Test whether an InstallTrigger.enabled is working
add_task(function * () {
  let testtab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "bug638292.html");

  function* verify(link, button) {
    info("Clicking " + link);

    let waitForNewTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

    yield BrowserTestUtils.synthesizeMouseAtCenter("#" + link, { button },
                                                   gBrowser.selectedBrowser);

    let newtab = yield waitForNewTabPromise;

    yield BrowserTestUtils.browserLoaded(newtab.linkedBrowser);

    let result = yield ContentTask.spawn(newtab.linkedBrowser, { }, function* () {
      return (content.document.getElementById("enabled").textContent == "true");
    });

    ok(result, "installTrigger for " + link + " should have been enabled");

    // Focus the old tab (link3 is opened in the background)
    if (link != "link3") {
      yield BrowserTestUtils.switchTab(gBrowser, testtab);
    }
    gBrowser.removeTab(newtab);
  }

  yield* verify("link1", 0);
  yield* verify("link2", 0);
  yield* verify("link3", 1);

  gBrowser.removeCurrentTab();
});


