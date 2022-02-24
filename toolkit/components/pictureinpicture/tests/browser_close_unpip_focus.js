/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that closing a pip window will not focus on the originating video's window.
add_task(async function test_close_button_focus() {
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
  let oldFocus = win1.focus;
  win1.focus = () => {
    ok(false, "Window is not supposed to be focused on");
  };
  EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
  await pipClosed;
  ok(true, "Window did not get focus");

  win1.focus = oldFocus;
  // close windows
  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});

// Tests that pressing the unpip button will focus on the originating video's window.
add_task(async function test_unpip_button_focus() {
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
  let closeButton = pipWin.document.getElementById("unpip");
  let pipWinFocusedPromise = BrowserTestUtils.waitForEvent(win1, "focus", true);
  EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);

  await pipClosed;
  await pipWinFocusedPromise;
  ok(true, "Originating window got focus");

  // close windows
  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});
