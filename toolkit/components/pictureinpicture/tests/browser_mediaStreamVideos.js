/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that the media stream video format has functional
 * support for PiP
 */
add_task(async function test_mediaStreamVideos() {
  await testToggle(
    TEST_ROOT + "test-media-stream.html",
    {
      "with-controls": { canToggle: true },
      "no-controls": { canToggle: true },
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        // Construct a new video element, and capture a stream from it
        // to redirect to both testing videos. Create the captureStreams after
        // we have metadata so tracks are immediately available, but wait with
        // playback until the setup is done.

        function logEvent(element, ev) {
          element.addEventListener(ev, () =>
            info(
              `${element.id} got event ${ev}. currentTime=${element.currentTime}`
            )
          );
        }

        const newVideo = content.document.createElement("video");
        newVideo.id = "new-video";
        newVideo.src = "test-video.mp4";
        newVideo.preload = "auto";
        logEvent(newVideo, "timeupdate");
        logEvent(newVideo, "ended");
        content.document.body.appendChild(newVideo);
        await ContentTaskUtils.waitForEvent(newVideo, "loadedmetadata");

        const mediastreamPlayingPromises = [];
        for (let videoID of ["with-controls", "no-controls"]) {
          const testedVideo = content.document.createElement("video");
          testedVideo.id = videoID;
          testedVideo.srcObject = newVideo.mozCaptureStream();
          testedVideo.play();
          mediastreamPlayingPromises.push(
            new Promise(r => (testedVideo.onplaying = r))
          );
          content.document.body.prepend(testedVideo);
        }

        await newVideo.play();
        await Promise.all(mediastreamPlayingPromises);
        newVideo.pause();
      });
    }
  );
});
