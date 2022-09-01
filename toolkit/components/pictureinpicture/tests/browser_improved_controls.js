/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the functionality of improved Picture-in-picture
 * playback controls.
 */
add_task(async () => {
  let videoID = "with-controls";

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
      let seekForwardButton = pipWin.document.getElementById("seekForward");
      let seekBackwardButton = pipWin.document.getElementById("seekBackward");

      // Try seek forward button
      let seekedForwardPromise = waitForVideoEvent("seeked");
      EventUtils.synthesizeMouseAtCenter(seekForwardButton, {}, pipWin);
      ok(await seekedForwardPromise, "The Forward button triggers");

      // Try seek backward button
      let seekedBackwardPromise = waitForVideoEvent("seeked");
      EventUtils.synthesizeMouseAtCenter(seekBackwardButton, {}, pipWin);
      ok(await seekedBackwardPromise, "The Backward button triggers");

      // The Fullsreen button appears when the pref is enabled and the fullscreen hidden property is set to false
      Assert.ok(!fullscreenButton.hidden, "The Fullscreen button is visible");

      // The seek Forward button appears when the pref is enabled and the seek forward button hidden property is set to false
      Assert.ok(!seekForwardButton.hidden, "The Forward button is visible");

      // The seek Backward button appears when the pref is enabled and the seek backward button hidden property is set to false
      Assert.ok(!seekBackwardButton.hidden, "The Backward button is visible");

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
      seekForwardButton = pipWin.document.getElementById("seekForward");
      seekBackwardButton = pipWin.document.getElementById("seekBackward");

      // The Fullsreen button disappears when the pref is disabled and the fullscreen hidden property is set to true
      Assert.ok(
        fullscreenButton.hidden,
        "The Fullscreen button is not visible"
      );

      // The seek Forward button disappears when the pref is disabled and the seek forward button hidden property is set to true
      Assert.ok(seekForwardButton.hidden, "The Forward button is not visible");

      // The seek Backward button disappears when the pref is disabled and the seek backward button hidden property is set to true
      Assert.ok(
        seekBackwardButton.hidden,
        "The Backward button is not visible"
      );
    }
  );
});
