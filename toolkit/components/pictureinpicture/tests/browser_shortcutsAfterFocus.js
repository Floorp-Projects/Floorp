/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests functionality of arrow keys in Picture-in-Picture window
 * for seeking and volume adjustment
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.keyboard-controls.enabled",
        true,
      ],
    ],
  });
  let videoID = "with-controls";
  info(`Testing ${videoID} case.`);

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let waitForVideoEvent = eventType => {
        return BrowserTestUtils.waitForContentEvent(browser, eventType, true);
      };

      await ensureVideosReady(browser);

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // run the next tests 4 times to ensure that they work for each PiP button, including none
      for (var i = 0; i < 4; i++) {
        // Try seek forward
        let seekedForwardPromise = waitForVideoEvent("seeked");
        EventUtils.synthesizeKey("KEY_ArrowRight", {}, pipWin);
        ok(await seekedForwardPromise, "The time seeked forward");

        // Try seek backward
        let seekedBackwardPromise = waitForVideoEvent("seeked");
        EventUtils.synthesizeKey("KEY_ArrowLeft", {}, pipWin);
        ok(await seekedBackwardPromise, "The time seeked backward");

        // Try volume down
        let volumeDownPromise = waitForVideoEvent("volumechange");
        EventUtils.synthesizeKey("KEY_ArrowDown", {}, pipWin);
        ok(await volumeDownPromise, "The volume went down");

        // Try volume up
        let volumeUpPromise = waitForVideoEvent("volumechange");
        EventUtils.synthesizeKey("KEY_ArrowUp", {}, pipWin);
        ok(await volumeUpPromise, "The volume went up");

        // Tab to get to the next button
        EventUtils.synthesizeKey("KEY_Tab", {}, pipWin);
      }

      await BrowserTestUtils.closeWindow(pipWin);
    }
  );
});
