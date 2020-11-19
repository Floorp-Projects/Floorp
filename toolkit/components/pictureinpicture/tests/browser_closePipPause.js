/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that MediaStream videos are not paused when closing
 * the PiP window.
 */
add_task(async function test_close_mediaStreamVideos() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_ROOT + "test-media-stream.html",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        // Construct a new video element, and capture a stream from it
        // to redirect to both testing videos
        let newVideo = content.document.createElement("video");
        newVideo.src = "test-video.mp4";
        newVideo.id = "media-stream-video";
        content.document.body.appendChild(newVideo);
        newVideo.loop = true;
      });
      await ensureVideosReady(browser);

      // Modify both the "with-controls" and "no-controls" videos so that they mirror
      // the new video that we just added via MediaStream.
      await SpecialPowers.spawn(browser, [], async () => {
        let newVideo = content.document.getElementById("media-stream-video");
        newVideo.play();

        for (let videoID of ["with-controls", "no-controls"]) {
          let testedVideo = content.document.createElement("video");
          testedVideo.id = videoID;
          testedVideo.srcObject = newVideo.mozCaptureStream().clone();
          content.document.body.prepend(testedVideo);
          if (
            testedVideo.readyState < content.HTMLMediaElement.HAVE_ENOUGH_DATA
          ) {
            info(`Waiting for 'canplaythrough' for '${testedVideo.id}'`);
            await ContentTaskUtils.waitForEvent(testedVideo, "canplaythrough");
          }
          testedVideo.play();
        }
      });

      for (let videoID of ["with-controls", "no-controls"]) {
        let pipWin = await triggerPictureInPicture(browser, videoID);
        ok(pipWin, "Got Picture-in-Picture window.");
        ok(
          !(await isVideoPaused(browser, videoID)),
          "The video is not paused in PiP window."
        );

        let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
        let closeButton = pipWin.document.getElementById("close");
        EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
        await pipClosed;
        ok(
          !(await isVideoPaused(browser, videoID)),
          "The video is not paused after closing PiP window."
        );
      }
    }
  );
});
