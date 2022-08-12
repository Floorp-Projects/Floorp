/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-In-Picture fullscreen button appears
 * when pref is enabled
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
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      const IMPROVED_CONTROLS_ENABLED_PREF =
        "media.videocontrols.picture-in-picture.improved-video-controls.enabled";

      await SpecialPowers.pushPrefEnv({
        set: [[IMPROVED_CONTROLS_ENABLED_PREF, true]],
      });

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let fullscreenButton = pipWin.document.getElementById("fullscreen");

      // The Fullsreen button appears when the pref is enabled and the fullscreen hidden property is set to false
      Assert.ok(!fullscreenButton.hidden, "The Fullscreen button is visible");

      // CLose the PIP window
      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      await pipClosed;

      await SpecialPowers.pushPrefEnv({
        set: [[IMPROVED_CONTROLS_ENABLED_PREF, false]],
      });

      // Open the video in PiP
      pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      fullscreenButton = pipWin.document.getElementById("fullscreen");

      // The Fullsreen button disappears when the pref is disabled and the fullscreen hidden property is set to true
      Assert.ok(
        fullscreenButton.hidden,
        "The Fullscreen button is not visible"
      );
    }
  );
});
