/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if a pre-existing Picture-in-Picture window exists, and a
 * different video is requested to open in Picture-in-Picture, that the
 * original Picture-in-Picture window closes and a new one is opened.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let pipWin = await triggerPictureInPicture(browser, "with-controls");
      ok(pipWin, "Got Picture-in-Picture window.");

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let pipWin2 = await triggerPictureInPicture(browser, "no-controls");
      await pipClosed;
      ok(true, "Original Picture-in-Picture window closed.");

      pipClosed = BrowserTestUtils.domWindowClosed(pipWin2);
      pipWin2.close();
      await pipClosed;
    }
  );
});
