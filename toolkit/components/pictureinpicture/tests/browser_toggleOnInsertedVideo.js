/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture toggle correctly attaches itself when the
 * video element has been inserted into the DOM after the video is ready to
 * play.
 */
add_task(async () => {
  const PAGE = TEST_ROOT + "test-page.html";

  await testToggle(
    PAGE,
    {
      inserted: { canToggle: true },
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let doc = content.document;

        // To avoid issues with the video not being scrolled into view, get
        // rid of the other videos on the page.
        let preExistingVideos = doc.querySelectorAll("video");
        for (let video of preExistingVideos) {
          video.remove();
        }

        let newVideo = doc.createElement("video");
        const { ContentTaskUtils } = ChromeUtils.import(
          "resource://testing-common/ContentTaskUtils.jsm"
        );
        let ready = ContentTaskUtils.waitForEvent(newVideo, "canplay");
        newVideo.src = "test-video.mp4";
        newVideo.id = "inserted";
        await ready;
        doc.body.appendChild(newVideo);
      });
    }
  );
});
