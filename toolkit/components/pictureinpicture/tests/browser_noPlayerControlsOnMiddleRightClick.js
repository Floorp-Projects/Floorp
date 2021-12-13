/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that no player controls are triggered by middle or
 * right click.
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
      await ensureVideosReady(browser);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);

      let playPause = pipWin.document.getElementById("playpause");
      let audioButton = pipWin.document.getElementById("audio");

      // Middle click the pause button
      EventUtils.synthesizeMouseAtCenter(playPause, { button: 1 }, pipWin);
      ok(!(await isVideoPaused(browser, videoID)), "The video is not paused.");

      // Right click the pause button
      EventUtils.synthesizeMouseAtCenter(playPause, { button: 2 }, pipWin);
      ok(!(await isVideoPaused(browser, videoID)), "The video is not paused.");

      // Middle click the mute button
      EventUtils.synthesizeMouseAtCenter(audioButton, { button: 1 }, pipWin);
      ok(!(await isVideoMuted(browser, videoID)), "The audio is not muted.");

      // Right click the mute button
      EventUtils.synthesizeMouseAtCenter(audioButton, { button: 2 }, pipWin);
      ok(!(await isVideoMuted(browser, videoID)), "The audio is not muted.");
    }
  );
});
