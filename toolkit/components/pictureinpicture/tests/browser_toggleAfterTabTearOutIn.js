/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Helper function that tries to use the mouse to open the Picture-in-Picture
 * player window for a video with and without the built-in controls.
 *
 * @param {Element} tab The tab to be tested.
 * @return Promise
 * @resolves When the toggles for both the video-with-controls and
 * video-without-controls have been tested.
 */
async function testToggleForTab(tab) {
  for (let videoID of ["with-controls", "no-controls"]) {
    let browser = tab.linkedBrowser;
    info(`Testing ${videoID} case.`);

    await testToggleHelper(browser, videoID, true);
  }
}

/**
 * Tests that the Picture-in-Picture toggle still works after tearing out the
 * tab into a new window, or tearing in a tab from one window to another.
 */
add_task(async () => {
  // The startingTab will be torn out and placed in the new window.
  let startingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE
  );

  // Tear out the starting tab into its own window...
  let newWinLoaded = BrowserTestUtils.waitForNewWindow();
  let win2 = gBrowser.replaceTabWithWindow(startingTab);
  await newWinLoaded;

  // Let's maximize the newly opened window so we don't have to worry about
  // the videos being visible.
  if (win2.windowState != win2.STATE_MAXIMIZED) {
    let resizePromise = BrowserTestUtils.waitForEvent(win2, "resize");
    win2.maximize();
    await resizePromise;
  }

  await SimpleTest.promiseFocus(win2);
  await testToggleForTab(win2.gBrowser.selectedTab);

  // Now bring the tab back to the original window.
  let dragInTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gBrowser.swapBrowsersAndCloseOther(dragInTab, win2.gBrowser.selectedTab);
  await SimpleTest.promiseFocus(window);
  await testToggleForTab(dragInTab);

  BrowserTestUtils.removeTab(dragInTab);
});
