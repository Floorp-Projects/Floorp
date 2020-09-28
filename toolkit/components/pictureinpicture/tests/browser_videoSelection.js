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

      // Get the video ID of either the first unpaused video || the longest video
      let pipVideoID = await SpecialPowers.spawn(browser, [], () => {
        let videoList = content.document.querySelectorAll("video");
        let longestDuration = -1;
        let pipVideoID = null;

        // Since all videos on the page start paused, the longest video's ID will be returned
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

      // Synthesize keyboard shortcut to open PiP window
      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("]", {
          accelKey: true,
          shiftKey: true,
          altKey: true,
        });
      } else {
        EventUtils.synthesizeKey("]", { accelKey: true, shiftKey: true });
      }

      // Wait for the PiP window to be opened and ready
      let pipWin = await domWindowOpened;
      await videoReady;

      ok(pipWin, "Got Picture-in-Picture window.");

      // Ensure the apropriate video is displaying the PiP window message and then close the PiP window
      await ensureMessageAndClosePiP(browser, pipVideoID, pipWin, false);

      // Get the video ID of either the first unpaused video || the longest video
      pipVideoID = await SpecialPowers.spawn(browser, [], () => {
        let videoList = content.document.querySelectorAll("video");
        videoList[1].play();
        videoList[2].play();
        let longestDuration = -1;
        let pipVideoID = null;

        // Several videos are unpaused, the id of the first unpaused video should be returned.
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

      // Remake domWindowOpened and videoReady w/ new video ID
      domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      videoReady = SpecialPowers.spawn(browser, [pipVideoID], async videoID => {
        let video = content.document.getElementById(videoID);
        await ContentTaskUtils.waitForCondition(() => {
          return video.isCloningElementVisually;
        }, "Video is being cloned visually.");
      });

      // Synthesize keyboard shortcut to open PiP window
      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("]", {
          accelKey: true,
          shiftKey: true,
          altKey: true,
        });
      } else {
        EventUtils.synthesizeKey("]", { accelKey: true, shiftKey: true });
      }

      // Wait for the PiP window to be opened and ready
      pipWin = await domWindowOpened;
      await videoReady;

      ok(pipWin, "Got Picture-in-Picture window.");

      // Ensure the apropriate video is displaying the PiP window message and then close the PiP window
      await ensureMessageAndClosePiP(browser, pipVideoID, pipWin, false);
    }
  );
});
