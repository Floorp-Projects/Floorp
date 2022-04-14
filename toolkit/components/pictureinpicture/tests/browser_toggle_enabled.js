/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const videoID = "with-controls";
const TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";
const PIP_ENABLED_PREF = "media.videocontrols.picture-in-picture.enabled";
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
 * Tests if the toggle is available depending on prefs
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let videoRect = await SpecialPowers.spawn(
        browser,
        [videoID],
        async videoID => {
          let video = content.document.getElementById(videoID);
          let rect = video.getBoundingClientRect();

          return {
            top: rect.top,
            left: rect.left,
            width: rect.width,
            height: rect.height,
          };
        }
      );
      // both toggle and pip true
      await SpecialPowers.pushPrefEnv({
        set: [
          [TOGGLE_ENABLED_PREF, true],
          [PIP_ENABLED_PREF, true],
        ],
      });

      let rect = await getToggleClientRect(browser, videoID);

      Assert.ok(!isVideoRect(videoRect, rect), "Toggle is showing");

      // only toggle false
      await SpecialPowers.pushPrefEnv({
        set: [[TOGGLE_ENABLED_PREF, false]],
      });

      rect = await getToggleClientRect(browser, videoID);
      Assert.ok(isVideoRect(videoRect, rect), "The toggle is not showing");

      // only pip false
      await SpecialPowers.pushPrefEnv({
        set: [
          [TOGGLE_ENABLED_PREF, true],
          [PIP_ENABLED_PREF, false],
        ],
      });

      rect = await getToggleClientRect(browser, videoID);
      Assert.ok(isVideoRect(videoRect, rect), "The toggle is not showing");

      // both toggle and pip false
      await SpecialPowers.pushPrefEnv({
        set: [
          [TOGGLE_ENABLED_PREF, false],
          [PIP_ENABLED_PREF, false],
        ],
      });

      rect = await getToggleClientRect(browser, videoID);
      Assert.ok(isVideoRect(videoRect, rect), "The toggle is not showing");
    }
  );
});
