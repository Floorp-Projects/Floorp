/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if the tab that's hosting a <video> that's opened in a
 * Picture-in-Picture window is closed, that the Picture-in-Picture
 * window is also closed.
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
    let browser = tab.linkedBrowser;

    let pipWin = await triggerPictureInPicture(browser, videoID);
    ok(pipWin, "Got Picture-in-Picture window.");

    let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
    BrowserTestUtils.removeTab(tab);
    await pipClosed;
  }
});
