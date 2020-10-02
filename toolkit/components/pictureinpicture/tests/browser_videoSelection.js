/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the correct video is opened in the
 * Picture-in-Picture player when opened via keyboard shortcut.
 * The shortcut will open the first unpaused video
 * or the longest video on the page.
 */
add_task(async function test_video_selection() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_ROOT + "test-video-selection.html",
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      let pipVideoID = await SpecialPowers.spawn(browser, [], () => {
        let videoList = content.document.querySelectorAll("video");
        let longestDuration = -1;
        let pipVideoID = null;

        for (let video of videoList) {
          if (!video.paused) {
            pipVideoID = video.id;
            break;
          }
          if (video.duration > longestDuration) {
            pipVideoID = video.id;
            longestDuration = video.duration;
          }
        }
        return pipVideoID;
      });

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      let videoReady = SpecialPowers.spawn(
        browser,
        [pipVideoID],
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

      await ensureMessageAndClosePiP(browser, pipVideoID, pipWin, false);

      pipVideoID = await SpecialPowers.spawn(browser, [], () => {
        let videoList = content.document.querySelectorAll("video");
        videoList[1].play();
        videoList[2].play();
        let longestDuration = -1;
        let pipVideoID = null;

        for (let video of videoList) {
          if (!video.paused) {
            pipVideoID = video.id;
            break;
          }
          if (video.duration > longestDuration) {
            pipVideoID = video.id;
            longestDuration = video.duration;
          }
        }
        return pipVideoID;
      });

      domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      videoReady = SpecialPowers.spawn(browser, [pipVideoID], async videoID => {
        let video = content.document.getElementById(videoID);
        await ContentTaskUtils.waitForCondition(() => {
          return video.isCloningElementVisually;
        }, "Video is being cloned visually.");
      });

      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("]", {
          accelKey: true,
          shiftKey: true,
          altKey: true,
        });
      } else {
        EventUtils.synthesizeKey("]", { accelKey: true, shiftKey: true });
      }

      pipWin = await domWindowOpened;
      await videoReady;

      ok(pipWin, "Got Picture-in-Picture window.");

      await ensureMessageAndClosePiP(browser, pipVideoID, pipWin, false);
    }
  );
});
