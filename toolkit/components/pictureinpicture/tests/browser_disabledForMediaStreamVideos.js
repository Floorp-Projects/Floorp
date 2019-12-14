/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Due to bug 1592539, we've disabled the Picture-in-Picture toggle and
 * context menu item for <video> elements with srcObject set to anything
 * other than null. We will re-enable Picture-in-Picture support for these
 * these types of video elements when the underlying bug is fixed.
 */
add_task(async function test_disabledForMediaStreamVideos() {
  // Disabling of the toggle on MediaStream videos is overridden by the always-show
  // pref.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.video-toggle.always-show",
        false,
      ],
    ],
  });

  await testToggle(
    TEST_PAGE,
    {
      "with-controls": { canToggle: false },
      "no-controls": { canToggle: false },
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        // Construct a new video element, and capture a stream from it
        // to redirect to both testing videos
        let newVideo = content.document.createElement("video");
        newVideo.src = "test-video.mp4";
        content.document.body.appendChild(newVideo);
        newVideo.loop = true;
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
