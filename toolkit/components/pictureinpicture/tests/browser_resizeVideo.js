/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Global values for the left and top edge pixel coordinates. These will be written to
// during the add_setup function in this test file.
let gLeftEdge = 0;
let gTopEdge = 0;

/**
 * Run the resize test on a player window.
 *
 * @param browser (xul:browser)
 *   The browser that has the source video.
 *
 * @param videoID (string)
 *   The id of the video in the browser to test.
 *
 * @param pipWin (player window)
 *   A player window to run the tests on.
 *
 * @param opts (object)
 *   The options for the test.
 *
 *   pinX (boolean):
 *     If true, the video's X position shouldn't change when resized.
 *
 *   pinY (boolean):
 *     If true, the video's Y position shouldn't change when resized.
 */
async function testVideo(browser, videoID, pipWin, { pinX, pinY } = {}) {
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

  /**
   * Check the new screen position against the previous one. When
   * pinX or pinY is true then the top left corner is checked in that
   * dimension. Otherwise, the bottom right corner is checked.
   *
   * The video position is determined by the screen edge it's closest
   * to, so in the default bottom right its bottom right corner should
   * match the previous video's bottom right corner. For the top left,
   * the top left corners should match.
   */
  function checkPosition(
    previousScreenX,
    previousScreenY,
    previousWidth,
    previousHeight,
    newScreenX,
    newScreenY,
    newWidth,
    newHeight
  ) {
    if (pinX || previousScreenX == gLeftEdge) {
      Assert.equal(
        previousScreenX,
        newScreenX,
        "New video is still in the same X position"
      );
    } else {
      Assert.less(
        Math.abs(previousScreenX + previousWidth - (newScreenX + newWidth)),
        2,
        "New video ends at the same screen X position (within 1 pixel)"
      );
    }
    if (pinY) {
      Assert.equal(
        previousScreenY,
        newScreenY,
        "New video is still in the same Y position"
      );
    } else {
      Assert.equal(
        previousScreenY + previousHeight,
        newScreenY + newHeight,
        "New video ends at the same screen Y position"
      );
    }
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

  // Store the window position for later.
  let initialScreenX = pipWin.mozInnerScreenX;
  let initialScreenY = pipWin.mozInnerScreenY;

  await switchVideoSource("test-video-cropped.mp4");

  let resizedWidth = pipWin.innerWidth;
  let resizedHeight = pipWin.innerHeight;
  let resizedAspectRatio = resizedWidth / resizedHeight;
  Assert.equal(
    Math.floor(resizedAspectRatio * 100),
    133, // 4 / 3 = 1.333333333
    "Resized aspect ratio is 4:3"
  );
  Assert.less(resizedWidth, initialWidth, "Resized video has smaller width");
  Assert.equal(
    resizedHeight,
    initialHeight,
    "Resized video is the same vertically"
  );

  let resizedScreenX = pipWin.mozInnerScreenX;
  let resizedScreenY = pipWin.mozInnerScreenY;
  checkPosition(
    initialScreenX,
    initialScreenY,
    initialWidth,
    initialHeight,
    resizedScreenX,
    resizedScreenY,
    resizedWidth,
    resizedHeight
  );

  await switchVideoSource("test-video-vertical.mp4");

  let verticalWidth = pipWin.innerWidth;
  let verticalHeight = pipWin.innerHeight;
  let verticalAspectRatio = verticalWidth / verticalHeight;

  if (verticalWidth == 136) {
    // The video is minimun width allowed
    Assert.equal(
      Math.floor(verticalAspectRatio * 100),
      56, // 1 / 2 = 0.5
      "Vertical aspect ratio is 1:2"
    );
  } else {
    Assert.equal(
      Math.floor(verticalAspectRatio * 100),
      50, // 1 / 2 = 0.5
      "Vertical aspect ratio is 1:2"
    );
  }

  Assert.less(verticalWidth, resizedWidth, "Vertical video width shrunk");
  Assert.equal(
    verticalHeight,
    initialHeight,
    "Vertical video height matches previous height"
  );

  let verticalScreenX = pipWin.mozInnerScreenX;
  let verticalScreenY = pipWin.mozInnerScreenY;
  checkPosition(
    resizedScreenX,
    resizedScreenY,
    resizedWidth,
    resizedHeight,
    verticalScreenX,
    verticalScreenY,
    verticalWidth,
    verticalHeight
  );

  await switchVideoSource("test-video.mp4");

  let restoredWidth = pipWin.innerWidth;
  let restoredHeight = pipWin.innerHeight;
  let restoredAspectRatio = restoredWidth / restoredHeight;
  Assert.equal(
    Math.floor(restoredAspectRatio * 100),
    177,
    "Restored aspect ratio is still 16:9"
  );
  Assert.less(
    Math.abs(initialWidth - pipWin.innerWidth),
    2,
    "Restored video has its original width"
  );
  Assert.equal(
    initialHeight,
    pipWin.innerHeight,
    "Restored video has its original height"
  );

  let restoredScreenX = pipWin.mozInnerScreenX;
  let restoredScreenY = pipWin.mozInnerScreenY;
  checkPosition(
    initialScreenX,
    initialScreenY,
    initialWidth,
    initialHeight,
    restoredScreenX,
    restoredScreenY,
    restoredWidth,
    restoredHeight
  );
}

add_setup(async () => {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      // Reset the saved PiP location to top-left edge of the screen, wherever
      // that may be. We record the top-left edge of the screen coordinates into
      // global variables to do later coordinate comparisons after resizes.
      let clearWin = await triggerPictureInPicture(browser, "with-controls");
      let initialScreenX = clearWin.mozInnerScreenX;
      let initialScreenY = clearWin.mozInnerScreenY;
      let PiPScreen = PictureInPicture.getWorkingScreen(
        initialScreenX,
        initialScreenY
      );
      [gLeftEdge, gTopEdge] = PictureInPicture.getAvailScreenSize(PiPScreen);
      clearWin.moveTo(gLeftEdge, gTopEdge);

      await BrowserTestUtils.closeWindow(clearWin);
    }
  );
});

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

        await testVideo(browser, videoID, pipWin);

        pipWin.moveTo(gLeftEdge, gTopEdge);

        await testVideo(browser, videoID, pipWin, { pinX: true, pinY: true });

        await BrowserTestUtils.closeWindow(pipWin);
      }
    );
  }
});

/**
 * Tests that the RTL video starts on the left and is pinned in the X dimension.
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({ set: [["intl.l10n.pseudo", "bidi"]] });

  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    await BrowserTestUtils.withNewTab(
      {
        url: TEST_PAGE,
        gBrowser,
      },
      async browser => {
        let pipWin = await triggerPictureInPicture(browser, videoID);

        await testVideo(browser, videoID, pipWin, { pinX: true });

        await BrowserTestUtils.closeWindow(pipWin);
      }
    );
  }
  await SpecialPowers.popPrefEnv();
});
