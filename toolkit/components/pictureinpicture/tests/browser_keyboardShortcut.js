/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if the user keys in the keyboard shortcut for
 * Picture-in-Picture, then the first video on the currently
 * focused page will be opened in the player window.
 */
add_task(async function test_pip_keyboard_shortcut() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      // In test-page.html, the "with-controls" video is the first one that
      // appears in the DOM, so this is what we expect to open via the keyboard
      // shortcut.
      const VIDEO_ID = "with-controls";

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      let videoReady = SpecialPowers.spawn(
        browser,
        [VIDEO_ID],
        async videoID => {
          let video = content.document.getElementById(videoID);
          await ContentTaskUtils.waitForCondition(() => {
            return video.isCloningElementVisually;
          }, "Video is being cloned visually.");
        }
      );

      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("]", {
          accelKey: true,
          shiftKey: true,
          altKey: true,
        });
      } else {
        EventUtils.synthesizeKey("]", { accelKey: true, shiftKey: true });
      }

      let pipWin = await domWindowOpened;
      await videoReady;

      ok(pipWin, "Got Picture-in-Picture window.");

      await ensureMessageAndClosePiP(browser, VIDEO_ID, pipWin, false);

      // Reopen PiP Window
      pipWin = await triggerPictureInPicture(browser, VIDEO_ID);
      await videoReady;

      ok(pipWin, "Got Picture-in-Picture window.");

      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey(
          "]",
          {
            accelKey: true,
            shiftKey: true,
            altKey: true,
          },
          pipWin
        );
      } else {
        EventUtils.synthesizeKey(
          "]",
          { accelKey: true, shiftKey: true },
          pipWin
        );
      }

      await BrowserTestUtils.windowClosed(pipWin);

      ok(pipWin.closed, "Picture-in-Picture window closed.");
    }
  );
});
