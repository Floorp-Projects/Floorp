/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the subtitles button hides after switching to a video that does not have subtitles
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
        true,
      ],
    ],
  });

  let videoID = "with-controls";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_WEBVTT,
      gBrowser,
    },
    async browser => {
      await prepareVideosAndWebVTTTracks(browser, videoID);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // Need to make sure that the PiP window is at least the minimum height
      let multiplier = 1;
      while (true) {
        if (multiplier * pipWin.innerHeight > 325) {
          break;
        }
        multiplier += 0.5;
      }

      pipWin.moveTo(50, 50);
      pipWin.resizeTo(
        pipWin.innerWidth * multiplier,
        pipWin.innerHeight * multiplier
      );

      let subtitlesButton = pipWin.document.querySelector("#closed-caption");
      await TestUtils.waitForCondition(() => {
        return !subtitlesButton.hidden;
      }, "Waiting for subtitles button to show initially");
      ok(!subtitlesButton.hidden, "The subtitles button is showing");

      let emptied = SpecialPowers.spawn(browser, [{ videoID }], async args => {
        let video = content.document.getElementById(args.videoID);
        info("Waiting for emptied event to be called");
        await ContentTaskUtils.waitForEvent(video, "emptied");
      });

      await SpecialPowers.spawn(browser, [{ videoID }], async args => {
        let video = content.document.getElementById(args.videoID);
        video.setAttribute("src", video.src);
        let len = video.textTracks.length;
        for (let i = 0; i < len; i++) {
          video.removeChild(video.children[0]);
        }
        video.load();
      });

      await emptied;

      await TestUtils.waitForCondition(() => {
        return subtitlesButton.hidden;
      }, "Waiting for subtitles button to hide after it was showing");
      ok(subtitlesButton.hidden, "The subtitles button is not showing");

      await BrowserTestUtils.closeWindow(pipWin);
    }
  );
});

/**
 * Tests the the subtitles button shows after switching from a video with no subtitles to a video with subtitles
 */
add_task(async () => {
  const videoID = "with-controls";
  const videoID2 = "with-controls-no-tracks";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_WEBVTT,
      gBrowser,
    },
    async browser => {
      let pipWin = await triggerPictureInPicture(browser, videoID2);
      ok(pipWin, "Got Picture-in-Picture window.");

      // Need to make sure that the PiP window is at least the minimum height
      let multiplier = 1;
      while (true) {
        if (multiplier * pipWin.innerHeight > 325) {
          break;
        }
        multiplier += 0.5;
      }

      pipWin.moveTo(50, 50);
      pipWin.resizeTo(
        pipWin.innerWidth * multiplier,
        pipWin.innerHeight * multiplier
      );

      let subtitlesButton = pipWin.document.querySelector("#closed-caption");
      await TestUtils.waitForCondition(() => {
        return subtitlesButton.hidden;
      }, "Making sure the subtitles button is hidden initially");
      ok(subtitlesButton.hidden, "The subtitles button is not showing");

      await SpecialPowers.spawn(
        browser,
        [{ videoID, videoID2 }],
        async args => {
          let video2 = content.document.getElementById(args.videoID2);

          let track = video2.addTextTrack("captions", "English", "en");
          track.mode = "showing";
          track.addCue(
            new content.window.VTTCue(0, 12, "[Test] This is the first cue")
          );
          track.addCue(
            new content.window.VTTCue(18.7, 21.5, "This is the second cue")
          );

          video2.setAttribute("src", video2.src);
          video2.load();

          is(
            video2.textTracks.length,
            1,
            "Number of tracks loaded should be 1"
          );
          video2.play();
          video2.pause();
        }
      );

      subtitlesButton = pipWin.document.querySelector("#closed-caption");
      await TestUtils.waitForCondition(() => {
        return !subtitlesButton.hidden;
      }, "Waiting for the subtitles button to show after switching to a video with subtitles.");
      ok(!subtitlesButton.hidden, "The subtitles button is showing");

      await BrowserTestUtils.closeWindow(pipWin);
    }
  );
});
