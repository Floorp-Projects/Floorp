/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const videoID = "without-audio";
const MIN_DURATION_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.min-video-secs";
const ALWAYS_SHOW_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.always-show";
const ACCEPTABLE_DIFF = 1;

function checkDifference(actual, expected) {
  let diff = Math.abs(actual - expected);
  return diff <= ACCEPTABLE_DIFF;
}

function isVideoRect(videoRect, rect) {
  info(
    "Video rect and toggle rect will be the same if the toggle doesn't show"
  );
  info(`Video rect: ${JSON.stringify(videoRect)}`);
  info(`Toggle rect: ${JSON.stringify(rect)}`);
  return (
    checkDifference(videoRect.top, rect.top) &&
    checkDifference(videoRect.left, rect.left) &&
    checkDifference(videoRect.width, rect.width) &&
    checkDifference(videoRect.height, rect.height)
  );
}

/**
 * Tests if the toggle is available for a video without an audio track
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE_WITHOUT_AUDIO,
    },
    async browser => {
      let videoRect = await SpecialPowers.spawn(
        browser,
        [videoID],
        async videoID => {
          let video = content.document.getElementById(videoID);
          Assert.ok(!video.mozHasAudio, "Video does not have an audio track");
          let rect = video.getBoundingClientRect();

          return {
            top: rect.top,
            left: rect.left,
            width: rect.width,
            height: rect.height,
          };
        }
      );

      await SpecialPowers.pushPrefEnv({
        set: [
          [ALWAYS_SHOW_PREF, false], // don't always show, we're testing the display logic
          [MIN_DURATION_PREF, 3], // sample video is only 4s
        ],
      });

      let rect = await getToggleClientRect(browser, videoID);

      Assert.ok(!isVideoRect(videoRect, rect), "Toggle is showing");
    }
  );
});
