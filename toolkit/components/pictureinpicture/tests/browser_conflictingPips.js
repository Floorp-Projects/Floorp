/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * If multiple PiPs try to open in the same place, they should not overlap.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let firstPip = await triggerPictureInPicture(browser, "with-controls");
      ok(firstPip, "Got first PiP window");

      await ensureMessageAndClosePiP(browser, "with-controls", firstPip, false);
      info("Closed first PiP to save location");

      let secondPip = await triggerPictureInPicture(browser, "with-controls");
      ok(secondPip, "Got second PiP window");

      let thirdPip = await triggerPictureInPicture(browser, "no-controls");
      ok(thirdPip, "Got third PiP window");

      Assert.ok(
        secondPip.screenX != thirdPip.screenX ||
          secondPip.screenY != thirdPip.screenY,
        "Conflicting PiPs were successfully opened in different locations"
      );

      await ensureMessageAndClosePiP(
        browser,
        "with-controls",
        secondPip,
        false
      );
      info("Second PiP was still open and is now closed");

      await ensureMessageAndClosePiP(browser, "no-controls", thirdPip, false);
      info("Third PiP was still open and is now closed");
    }
  );
});
