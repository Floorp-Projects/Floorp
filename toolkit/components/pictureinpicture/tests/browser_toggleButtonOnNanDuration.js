/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that we do not show the Picture-in-Picture toggle on video
 * elements that have a NaN duration.
 */
add_task(async function test_toggleButtonOnNanDuration() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_NAN_VIDEO_DURATION,
      gBrowser,
    },
    async browser => {
      const VIDEO_ID = "test-video";

      await SpecialPowers.spawn(browser, [VIDEO_ID], async videoID => {
        let video = content.document.getElementById(videoID);
        if (video.readyState < content.HTMLMediaElement.HAVE_ENOUGH_DATA) {
          info(`Waiting for 'canplaythrough' for ${videoID}`);
          await ContentTaskUtils.waitForEvent(video, "canplaythrough");
        }
      });

      await testToggleHelper(browser, "nan-duration", false);

      await testToggleHelper(browser, "test-video", true);
    }
  );
});
