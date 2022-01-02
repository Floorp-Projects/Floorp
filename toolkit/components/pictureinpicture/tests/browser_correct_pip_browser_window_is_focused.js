/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that closing a pip window will focus its respective browser window.
add_task(async function() {
  // initialize
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  // Open PiP
  let videoID = "with-controls";
  let pipTab = await BrowserTestUtils.openNewForegroundTab(
    win1.gBrowser,
    TEST_PAGE
  );
  let browser = pipTab.linkedBrowser;
  let pipWin = await triggerPictureInPicture(browser, videoID);
  ok(pipWin, "Got Picture-in-Picture window.");
  let focus = BrowserTestUtils.waitForEvent(win2, "focus", true);
  win2.focus();
  await focus;
  // Close PiP
  let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
  let closeButton = pipWin.document.getElementById("close");
  let pipWinFocusedPromise = BrowserTestUtils.waitForEvent(win1, "focus", true); // store a reference to the promise
  EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
  await pipClosed;
  await pipWinFocusedPromise;
  ok(true, "Opening window got focus");
  // close windows
  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});
