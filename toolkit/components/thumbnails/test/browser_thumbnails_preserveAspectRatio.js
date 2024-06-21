/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PageThumbUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PageThumbUtils.sys.mjs"
);

/**
 * Returns the width & height of the content area for a given browser
 *
 * @param browser the browser to get dimensions for
 */
async function getContentDimensions(browser) {
  let thumbnailsActor =
    browser.browsingContext.currentWindowGlobal.getActor("Thumbnails");
  let contentInfo = await thumbnailsActor.sendQuery(
    "Browser:Thumbnail:ContentInfo"
  );
  return [contentInfo.width, contentInfo.height];
}

/**
 * Verify that preserveAspectRatio generates a sized canvas
 * that matches the window's aspect ratio
 *
 * @param windowWidth width of the browser window
 * @param windowHeight height of the browser window
 * @param useRemote Whether to use a remote or local browser (these use different PageThumbs APIs)
 * @param win the window to test
 */
async function testPreserveAspectRatio(
  windowWidth,
  windowHeight,
  useRemote,
  win
) {
  let resizeComplete = BrowserTestUtils.waitForEvent(win, "resize");
  win.resizeTo(windowWidth, windowHeight);
  await resizeComplete;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser: win.gBrowser,
      url: useRemote ? "https://example.com" : "about:about",
    },
    async browser => {
      let canvas = PageThumbUtils.createCanvas(win);
      let [, defaultThumbnailHeight] = PageThumbUtils.getThumbnailSize(win);

      await PageThumbs.captureToCanvas(browser, canvas, {
        targetWidth: 300,
        preserveAspectRatio: false,
      });

      Assert.equal(
        canvas.height,
        defaultThumbnailHeight,
        "canvas uses default thumbnail height"
      );

      await PageThumbs.captureToCanvas(browser, canvas, {
        targetWidth: 300,
        preserveAspectRatio: true,
      });

      let [contentWidth, contentHeight] = await getContentDimensions(browser);

      let aspectRatio = contentWidth / contentHeight;
      let expectedHeight = canvas.width / aspectRatio;

      // Calculation may be off by a pixel here and there due to floating point math;
      // assume it's correct if error is within 1%
      let deviation = Math.abs(expectedHeight - canvas.height) / expectedHeight;
      Assert.ok(deviation < 0.01, "canvas height within 1% of expected height");
    }
  );
}

/**
 * Tests PageThumbs' preserveAspectRatio option for a variety
 * of window aspect ratios.
 */
add_task(async function thumbnails_preserveAspectRatio() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await testPreserveAspectRatio(600, 900, true, win);
  await testPreserveAspectRatio(900, 600, true, win);
  await testPreserveAspectRatio(450, 1000, true, win);
  await testPreserveAspectRatio(600, 900, false, win);
  await testPreserveAspectRatio(900, 600, false, win);
  await testPreserveAspectRatio(450, 1000, false, win);
  await BrowserTestUtils.closeWindow(win);
});
