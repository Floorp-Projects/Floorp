/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
/**
 * This test creates a PiP window, then switches to another tab and confirms
 * that the PiP tab is still active.
 */
add_task(async () => {
  let videoID = "no-controls";
  let firstTab = gBrowser.selectedTab;
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let originatingTab = gBrowser.getTabForBrowser(browser);
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      await BrowserTestUtils.switchTab(gBrowser, firstTab);

      let switcher = gBrowser._getSwitcher();

      Assert.equal(
        switcher.getTabState(originatingTab),
        switcher.STATE_LOADED,
        "The originating browser tab should be in STATE_LOADED."
      );
      Assert.equal(
        browser.docShellIsActive,
        true,
        "The docshell should be active in the originating tab"
      );

      // We need to destroy the current AsyncTabSwitcher to avoid
      // tabrowser.shouldActivateDocShell going in the
      // AsyncTabSwitcher.shouldActivateDocShell code path which isn't PiP aware.
      switcher.destroy();

      // Closing with window.close doesn't actually pause the video, so click
      // the close button instead.
      pipWin.document.getElementById("close").click();
      await BrowserTestUtils.windowClosed(pipWin);

      Assert.equal(
        browser.docShellIsActive,
        false,
        "The docshell should be inactive in the originating tab"
      );
    }
  );
});

/**
 * This test creates a PiP window, then minimizes the browser and confirms
 * that the PiP tab is still active.
 */
add_task(async () => {
  let videoID = "no-controls";
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let originatingTab = gBrowser.getTabForBrowser(browser);
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
        window,
        "sizemodechange"
      );
      window.minimize();
      await promiseSizeModeChange;

      let switcher = gBrowser._getSwitcher();

      Assert.equal(
        switcher.getTabState(originatingTab),
        switcher.STATE_LOADED,
        "The originating browser tab should be in STATE_LOADED."
      );

      await BrowserTestUtils.closeWindow(pipWin);

      // Workaround bug 1782134.
      window.restore();
    }
  );
});
