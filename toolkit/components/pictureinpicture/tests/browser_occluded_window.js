/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the forceAppWindowActive flag is correctly set whenever a PiP window is opened
 * and closed across multiple tabs on the same browser window.
 */
add_task(async function forceActiveMultiPiPTabs() {
  let videoID = "with-controls";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      let bc = browser.ownerGlobal.browsingContext;
      info("is window active: " + bc.isActive);

      info("Opening new tab");
      let newTab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_PAGE
      );
      let newTabBrowser = newTab.linkedBrowser;
      await ensureVideosReady(newTabBrowser);

      ok(!bc.forceAppWindowActive, "Forced window active should be false");
      info("is window active: " + bc.isActive);

      info("Now opening PiP windows");
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      ok(
        bc.forceAppWindowActive,
        "Forced window active should be true since PiP is open"
      );
      info("is window active: " + bc.isActive);

      let newTabPiPWin = await triggerPictureInPicture(newTabBrowser, videoID);
      ok(newTabPiPWin, "Got Picture-in-Picture window in new tab");
      ok(
        bc.forceAppWindowActive,
        "Force window active should still be true after opening a new PiP window in new tab"
      );
      info("is window active: " + bc.isActive);

      let pipClosedNewTab = BrowserTestUtils.domWindowClosed(newTabPiPWin);
      let pipUnloadedNewTab = BrowserTestUtils.waitForEvent(
        newTabPiPWin,
        "unload"
      );
      let closeButtonNewTab = newTabPiPWin.document.getElementById("close");
      info("Selecting close button");
      EventUtils.synthesizeMouseAtCenter(closeButtonNewTab, {}, newTabPiPWin);
      info("Waiting for PiP window to close");
      await pipUnloadedNewTab;
      await pipClosedNewTab;

      ok(
        bc.forceAppWindowActive,
        "Force window active should still be true after removing new tab's PiP window"
      );

      info("is window active: " + bc.isActive);

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let pipUnloaded = BrowserTestUtils.waitForEvent(pipWin, "unload");
      let closeButton = pipWin.document.getElementById("close");
      info("Selecting close button");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      info("Waiting for PiP window to close");
      await pipUnloaded;
      await pipClosed;

      ok(
        !bc.forceAppWindowActive,
        "Force window active should now be false after removing the last PiP window"
      );

      info("is window active: " + bc.isActive);

      await BrowserTestUtils.removeTab(newTab);
    }
  );
});

/**
 * Tests that the forceAppWindowActive flag is correctly set when a tab with PiP enabled is
 * moved to another window.
 */
add_task(async function forceActiveMovePiPToWindow() {
  let videoID = "with-controls";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      info("Opening first tab");

      await ensureVideosReady(browser);

      let tab = gBrowser.getTabForBrowser(browser);
      let bc = browser.ownerGlobal.browsingContext;

      info("is window active: " + bc.isActive);

      ok(!bc.forceAppWindowActive, "Forced window active should be false");
      info("is window active: " + bc.isActive);

      info("Now opening PiP windows");
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window in first tab.");

      ok(
        bc.forceAppWindowActive,
        "Forced window active should be true since PiP is open"
      );
      info("is window active: " + bc.isActive);

      let swapDocShellsPromise = BrowserTestUtils.waitForEvent(
        browser,
        "SwapDocShells"
      );
      let tabClosePromise = BrowserTestUtils.waitForEvent(tab, "TabClose");
      let tabSwapPiPPromise = BrowserTestUtils.waitForEvent(
        tab,
        "TabSwapPictureInPicture"
      );
      info("Replacing tab with window");
      let newWindow = gBrowser.replaceTabWithWindow(tab);
      let newWinLoadedPromise = BrowserTestUtils.waitForEvent(
        newWindow,
        "load"
      );

      info("Waiting for new window to initialize after swap");
      await Promise.all([
        tabSwapPiPPromise,
        swapDocShellsPromise,
        tabClosePromise,
        newWinLoadedPromise,
      ]);

      let newWindowBC = newWindow.browsingContext;
      tab = newWindow.gBrowser.selectedTab;

      ok(
        !bc.forceAppWindowActive,
        "Force window active should no longer be true after moving the previous tab to a new window"
      );
      info("is window active: " + bc.isActive);
      ok(
        newWindowBC.forceAppWindowActive,
        "Force window active should be true for new window since PiP is open"
      );
      info("is secondary window active: " + newWindowBC.isActive);

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let pipUnloaded = BrowserTestUtils.waitForEvent(pipWin, "unload");
      let closeButton = pipWin.document.getElementById("close");
      info("Selecting close button");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      info("Waiting for PiP window to close");
      await pipUnloaded;
      await pipClosed;

      ok(
        !newWindowBC.forceAppWindowActive,
        "Force window active should now be false for new window after removing the last PiP window"
      );
      info("is secondary window active: " + newWindowBC.isActive);

      await BrowserTestUtils.removeTab(tab);
    }
  );
});

/**
 * Tests that the forceAppWindowActive flag is correctly set when multiple PiP
 * windows are created for a single PiP window.
 */
add_task(async function forceActiveMultiPiPSamePage() {
  let videoID1 = "with-controls";
  let videoID2 = "no-controls";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);
      let bc = browser.ownerGlobal.browsingContext;

      ok(
        !bc.forceAppWindowActive,
        "Forced window active should be false at the start of the test"
      );
      info("is window active: " + bc.isActive);

      let pipWin1 = await triggerPictureInPicture(browser, videoID1);
      ok(pipWin1, "Got Picture-in-Picture window 1.");

      ok(
        bc.forceAppWindowActive,
        "Forced window active should be true since PiP is open"
      );
      info("is window active: " + bc.isActive);

      let pipWin2 = await triggerPictureInPicture(browser, videoID2);
      ok(pipWin2, "Got Picture-in-Picture window 2.");

      ok(
        bc.forceAppWindowActive,
        "Forced window active should be true after opening another PiP window on the same page"
      );
      info("is window active: " + bc.isActive);

      let pipClosed1 = BrowserTestUtils.domWindowClosed(pipWin1);
      let pipUnloaded1 = BrowserTestUtils.waitForEvent(pipWin1, "unload");
      let closeButton1 = pipWin1.document.getElementById("close");
      info("Selecting close button");
      EventUtils.synthesizeMouseAtCenter(closeButton1, {}, pipWin1);
      info("Waiting for PiP window to close");
      await pipUnloaded1;
      await pipClosed1;

      ok(
        bc.forceAppWindowActive,
        "Force window active should still be true after removing PiP window 1"
      );
      info("is window active: " + bc.isActive);

      let pipClosed2 = BrowserTestUtils.domWindowClosed(pipWin2);
      let pipUnloaded2 = BrowserTestUtils.waitForEvent(pipWin2, "unload");
      let closeButton2 = pipWin2.document.getElementById("close");
      info("Selecting close button");
      EventUtils.synthesizeMouseAtCenter(closeButton2, {}, pipWin2);
      info("Waiting for PiP window to close");
      await pipUnloaded2;
      await pipClosed2;

      ok(
        !bc.forceAppWindowActive,
        "Force window active should now be false after removing PiP window 2"
      );
      info("is window active: " + bc.isActive);
    }
  );
});
