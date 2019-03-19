/**
 * Waits for a View Source window to be opened at a particular
 * URL.
 *
 * @param {string} expectedURL The view-source: URL that's expected.
 * @resolves {DOM Window} The window that was opened.
 * @returns {Promise}
 */
async function waitForNewViewSourceWindow(expectedURL) {
  let win = await BrowserTestUtils.domWindowOpened();
  await BrowserTestUtils.waitForEvent(win, "EndSwapDocShells", true);
  let browser = win.gBrowser.selectedBrowser;
  if (browser.currentURI.spec != expectedURL) {
    await BrowserTestUtils.browserLoaded(browser, false, expectedURL);
  }
  return win;
}

/**
 * When view_source.tab is set to false, view source should
 * open in new browser window instead of new tab.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["view_source.tab", false]],
  });

  const PAGE = "http://example.com/";
  await BrowserTestUtils.withNewTab({
    url: PAGE,
    gBrowser,
  }, async browser => {
    let winPromise = waitForNewViewSourceWindow("view-source:" + PAGE);
    BrowserViewSource(browser);
    let win = await winPromise;

    ok(win, "View Source opened up in a new window.");
    await BrowserTestUtils.closeWindow(win);
  });
});

/**
 * When view_source.tab is set to false, view partial source
 * should open up in new browser window instead of new tab.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["view_source.tab", false]],
  });

  const para = "<p>test</p>";
  const source = `<html><body>${para}</body></html>`;
  await BrowserTestUtils.withNewTab({
    url: "data:text/html," + source,
    gBrowser,
  }, async browser => {
    let winPromise = waitForNewViewSourceWindow("view-source:data:text/html;charset=utf-8,%3Cp%3E%EF%B7%90test%EF%B7%AF%3C%2Fp%3E");
    await ContentTask.spawn(gBrowser.selectedBrowser, null, async function(arg) {
      let element = content.document.querySelector("p");
      content.getSelection().selectAllChildren(element);
    });

    let contentAreaContextMenuPopup =
      document.getElementById("contentAreaContextMenu");
    let popupShownPromise =
      BrowserTestUtils.waitForEvent(contentAreaContextMenuPopup, "popupshown");
    await BrowserTestUtils.synthesizeMouseAtCenter("p",
      { type: "contextmenu", button: 2 }, gBrowser.selectedBrowser);
    await popupShownPromise;

    let popupHiddenPromise =
      BrowserTestUtils.waitForEvent(contentAreaContextMenuPopup, "popuphidden");
    let item = document.getElementById("context-viewpartialsource-selection");
    EventUtils.synthesizeMouseAtCenter(item, {});
    await popupHiddenPromise;
    dump("Before winPromise");
    let win = await winPromise;
    dump("After winPromise");
    ok(win, "View Partial Source opened up in a new window.");
    await BrowserTestUtils.closeWindow(win);
  });
});
