/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the visibility of the toggle will be
 * recomputed after durationchange events fire.
 */
add_task(async function test_durationChange() {
  // Most of the Picture-in-Picture tests run with the always-show
  // preference set to true to avoid the toggle visibility heuristics.
  // Since this test actually exercises those heuristics, we have
  // to temporarily disable that pref.
  //
  // We also reduce the minimum video length for displaying the toggle
  // to 5 seconds to avoid having to include or generate a 45 second long
  // video (which is the default minimum length).
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.video-toggle.always-show",
        false,
      ],
      ["media.videocontrols.picture-in-picture.video-toggle.min-video-secs", 5],
    ],
  });

  // First, ensure that the toggle doesn't show up for these
  // short videos by default.
  await testToggle(TEST_PAGE, {
    "with-controls": { canToggle: false },
    "no-controls": { canToggle: false },
  });

  // Now cause the video to change sources, which should fire a
  // durationchange event. The longer video should qualify us for
  // displaying the toggle.
  await testToggle(
    TEST_PAGE,
    {
      "with-controls": { canToggle: true },
      "no-controls": { canToggle: true },
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        for (let videoID of ["with-controls", "no-controls"]) {
          let video = content.document.getElementById(videoID);
          video.src = "gizmo.mp4";
          let durationChangePromise = ContentTaskUtils.waitForEvent(
            video,
            "durationchange"
          );

          video.load();
          await durationChangePromise;
        }
      });
    }
  );
});
