/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests to ensure that tabbing to the pip button and pressing space works
 * to open the picture-in-picture window.
 */
add_task(async () => {
  let videoID = "with-controls";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID, () => {
        EventUtils.synthesizeKey("KEY_Tab", {}); // play button
        EventUtils.synthesizeKey("KEY_Tab", {}); // pip button
        EventUtils.synthesizeKey(" ", {});
      });
      ok(pipWin, "Got Picture-in-Picture window.");

      await BrowserTestUtils.closeWindow(pipWin);
    }
  );
});
