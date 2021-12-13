/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests functionality of the various controls for the Picture-in-Picture
 * video window.
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.audio-toggle.enabled", true],
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
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      ok(!(await isVideoPaused(browser, videoID)), "The video is not paused.");

      let playPause = pipWin.document.getElementById("playpause");
      let audioButton = pipWin.document.getElementById("audio");

      // Try the pause button
      let pausedPromise = waitForVideoEvent("pause");
      EventUtils.synthesizeMouseAtCenter(playPause, {}, pipWin);
      await pausedPromise;
      ok(await isVideoPaused(browser, videoID), "The video is paused.");

      // Try the play button
      let playPromise = waitForVideoEvent("play");
      EventUtils.synthesizeMouseAtCenter(playPause, {}, pipWin);
      await playPromise;
      ok(!(await isVideoPaused(browser, videoID)), "The video is playing.");

      // Try the mute button
      let mutedPromise = waitForVideoEvent("volumechange");
      ok(!(await isVideoMuted(browser, videoID)), "The audio is playing.");
      EventUtils.synthesizeMouseAtCenter(audioButton, {}, pipWin);
      await mutedPromise;
      ok(await isVideoMuted(browser, videoID), "The audio is muted.");

      // Try the unmute button
      let unmutedPromise = waitForVideoEvent("volumechange");
      EventUtils.synthesizeMouseAtCenter(audioButton, {}, pipWin);
      await unmutedPromise;
      ok(!(await isVideoMuted(browser, videoID)), "The audio is playing.");

      // Try the unpip button.
      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let unpipButton = pipWin.document.getElementById("unpip");
      EventUtils.synthesizeMouseAtCenter(unpipButton, {}, pipWin);
      await pipClosed;
      ok(!(await isVideoPaused(browser, videoID)), "The video is not paused.");

      // Try the close button.
      pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      ok(!(await isVideoPaused(browser, videoID)), "The video is not paused.");

      pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      await pipClosed;
      ok(await isVideoPaused(browser, videoID), "The video is paused.");
    }
  );
});
