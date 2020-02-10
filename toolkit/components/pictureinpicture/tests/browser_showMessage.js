/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that triggering Picture-in-Picture causes the Picture-in-Picture
 * window to be opened, and a message to be displayed in the original video
 * player area. Also ensures that once the Picture-in-Picture window is closed,
 * the video goes back to the original state.
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    await BrowserTestUtils.withNewTab(
      {
        url: TEST_PAGE,
        gBrowser,
      },
      async browser => {
        let pipWin = await triggerPictureInPicture(browser, videoID);
        ok(pipWin, "Got Picture-in-Picture window.");

        try {
          await assertShowingMessage(browser, videoID, true);
        } finally {
          let uaWidgetUpdate = BrowserTestUtils.waitForContentEvent(
            browser,
            "UAWidgetSetupOrChange",
            true /* capture */
          );
          await BrowserTestUtils.closeWindow(pipWin);
          await uaWidgetUpdate;
        }
      }
    );
  }
});
