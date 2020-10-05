/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that the media stream video format has functional
 * support for PiP
 */
add_task(async function test_mediaStreamVideos() {
  await testToggle(
    TEST_PAGE,
    {
      "with-controls": { canToggle: true },
      "no-controls": { canToggle: true },
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        // Construct a new video element, and capture a stream from it
        // to redirect to both testing videos
        let newVideo = content.document.createElement("video");
        newVideo.src = "test-video.mp4";
        content.document.body.appendChild(newVideo);
        newVideo.loop = true;
      });
      await ensureVideosReady(browser);

      await SpecialPowers.spawn(browser, [], async () => {
        let newVideo = content.document.body.lastChild;
        await newVideo.play();

        for (let videoID of ["with-controls", "no-controls"]) {
          let testedVideo = content.document.getElementById(videoID);
          testedVideo.srcObject = newVideo.mozCaptureStream();
          await testedVideo.play();
          await testedVideo.pause();
        }
      });
    }
  );
});
