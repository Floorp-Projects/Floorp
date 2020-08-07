/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests functionality of the various controls for the Picture-in-Picture
 * video window.
 */
add_task(async () => {
  let videoID = "with-controls";
  info(`Testing ${videoID} case.`);

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let isVideoPaused = () => {
        return SpecialPowers.spawn(browser, [videoID], async videoID => {
          return content.document.getElementById(videoID).paused;
        });
      };
      let isVideoMuted = () => {
        return SpecialPowers.spawn(browser, [videoID], async videoID => {
          return content.document.getElementById(videoID).muted;
        });
      };

      await ensureVideosReady(browser);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      ok(!(await isVideoPaused()), "The video is not paused");

      let playPause = pipWin.document.getElementById("playpause");
      let audioButton = pipWin.document.getElementById("audio");

      // Try the pause button
      EventUtils.synthesizeMouseAtCenter(playPause, {}, pipWin);
      ok(await isVideoPaused(), "The video is paused");

      // Try the play button
      EventUtils.synthesizeMouseAtCenter(playPause, {}, pipWin);
      ok(!(await isVideoPaused()), "The video is playing");

      // Try the mute button
      ok(!(await isVideoMuted()), "The audio is playing");
      EventUtils.synthesizeMouseAtCenter(audioButton, {}, pipWin);
      ok(await isVideoMuted(), "The audio is muted");

      // Try the unmute button
      EventUtils.synthesizeMouseAtCenter(audioButton, {}, pipWin);
      ok(!(await isVideoMuted()), "The audio is playing");

      // Try the unpip button.
      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let unpipButton = pipWin.document.getElementById("unpip");
      EventUtils.synthesizeMouseAtCenter(unpipButton, {}, pipWin);
      await pipClosed;
      ok(!(await isVideoPaused()), "The video is not paused");

      // Try the close button.
      pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      ok(!(await isVideoPaused()), "The video is not paused");

      pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      await pipClosed;
      ok(await isVideoPaused(), "The video is paused");
    }
  );
});
