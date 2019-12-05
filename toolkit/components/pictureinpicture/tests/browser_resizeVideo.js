/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if a <video> element is resized the Picture-in-Picture window
 * will be resized to match the new dimensions.
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    await BrowserTestUtils.withNewTab(
      {
        url: TEST_PAGE,
        gBrowser,
      },
      async browser => {
        let pipWin = await triggerPictureInPicture(browser, videoID);

        async function switchVideoSource(src) {
          let videoResized = BrowserTestUtils.waitForEvent(pipWin, "resize");
          await ContentTask.spawn(
            browser,
            { src, videoID },
            async ({ src, videoID }) => {
              let doc = content.document;
              let video = doc.getElementById(videoID);
              video.src = src;
            }
          );
          await videoResized;
        }

        Assert.ok(pipWin, "Got PiP window.");

        let initialWidth = pipWin.innerWidth;
        let initialHeight = pipWin.innerHeight;
        let initialAspectRatio = initialWidth / initialHeight;
        Assert.equal(
          Math.floor(initialAspectRatio * 100),
          177, // 16 / 9 = 1.777777777
          "Original aspect ratio is 16:9"
        );

        await switchVideoSource("test-video-cropped.mp4");

        let resizedWidth = pipWin.innerWidth;
        let resizedHeight = pipWin.innerHeight;
        let resizedAspectRatio = resizedWidth / resizedHeight;
        Assert.equal(
          Math.floor(resizedAspectRatio * 100),
          133, // 4 / 3 = 1.333333333
          "Resized aspect ratio is 4:3"
        );
        Assert.equal(
          initialWidth,
          resizedWidth,
          "Resized video has the same width"
        );
        Assert.greater(
          resizedHeight,
          initialHeight,
          "Resized video grew vertically"
        );

        await switchVideoSource("test-video-vertical.mp4");

        let verticalAspectRatio = pipWin.innerWidth / pipWin.innerHeight;
        Assert.equal(
          Math.floor(verticalAspectRatio * 100),
          50, // 1 / 2 = 0.5
          "Vertical aspect ratio is 1:2"
        );
        Assert.less(
          pipWin.innerWidth,
          resizedWidth,
          "Vertical video width shrunk"
        );
        Assert.equal(
          resizedWidth,
          pipWin.innerHeight,
          "Vertical video height matches previous width"
        );

        await switchVideoSource("test-video.mp4");

        let restoredAspectRatio = pipWin.innerWidth / pipWin.innerHeight;
        Assert.equal(
          Math.floor(restoredAspectRatio * 100),
          177,
          "Restored aspect ratio is still 16:9"
        );
        Assert.equal(
          initialWidth,
          pipWin.innerWidth,
          "Restored video has its original width"
        );
        Assert.equal(
          initialHeight,
          pipWin.innerHeight,
          "Restored video has its original height"
        );

        await BrowserTestUtils.closeWindow(pipWin);
      }
    );
  }
});
