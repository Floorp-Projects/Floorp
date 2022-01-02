/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_pip_keyboard_shortcut_with_nan_video_duration() {
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

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);

      let videoReady = SpecialPowers.spawn(
        browser,
        [VIDEO_ID],
        async videoID => {
          let video = content.document.getElementById(videoID);
          await ContentTaskUtils.waitForCondition(() => {
            return video.isCloningElementVisually;
          }, "Video is being cloned visually.");
        }
      );

      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("]", {
          accelKey: true,
          shiftKey: true,
          altKey: true,
        });
      } else {
        EventUtils.synthesizeKey("]", { accelKey: true, shiftKey: true });
      }

      let pipWin = await domWindowOpened;
      await videoReady;

      ok(pipWin, "Got Picture-in-Picture window.");

      pipWin.close();
    }
  );
});
