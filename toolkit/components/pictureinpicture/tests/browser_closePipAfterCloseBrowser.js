/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the pip window closes after the parent browser window is closed
add_task(async function() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let videoID = "with-controls";
  let pipTab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    TEST_PAGE
  );

  let browser = pipTab.linkedBrowser;
  let pipWin = await triggerPictureInPicture(browser, videoID);
  let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
  ok(pipWin, "Got Picture-in-Picture window.");

  await BrowserTestUtils.closeWindow(win);
  await pipClosed;
});
