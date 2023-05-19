/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the F-key would enter and exit full screen mode in PiP for the default locale (en-US).
 */
add_task(async () => {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  let browser = tab.linkedBrowser;
  let videoID = "with-controls";

  let pipWin = await triggerPictureInPicture(browser, videoID);
  ok(pipWin, "The Picture-in-Picture window is there.");

  ok(
    !pipWin.document.fullscreenElement,
    "PiP should not yet be in fullscreen."
  );

  await promiseFullscreenEntered(pipWin, async () => {
    EventUtils.synthesizeKey("f", {}, pipWin);
  });

  ok(
    pipWin.document.fullscreenElement == pipWin.document.body,
    "F-key should have caused to enter fullscreen."
  );

  await promiseFullscreenExited(pipWin, async () => {
    EventUtils.synthesizeKey("f", {}, pipWin);
  });

  ok(
    !pipWin.document.fullscreenElement,
    "F-key should have caused to leave fullscreen."
  );

  await BrowserTestUtils.removeTab(tab);
});
