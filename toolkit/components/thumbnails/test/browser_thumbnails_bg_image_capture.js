/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const BASE_URL =
  "http://mochi.test:8888/browser/toolkit/components/thumbnails/";

/**
 * These tests ensure that when trying to capture a url that is an image file,
 * the image itself is captured instead of the the browser window displaying the
 * image, and that the thumbnail maintains the image aspect ratio.
 */
add_task(async function thumbnails_bg_image_capture() {
  // Test that malformed input causes _finishCurrentCapture to be called with
  // the correct reason.
  const emptyUrl = "data:text/plain,";
  await bgCapture(emptyUrl, {
    isImage: true,
    onDone: (url, reason) => {
      // BackgroundPageThumbs.TEL_CAPTURE_DONE_LOAD_FAILED === 6
      is(reason, 6, "Should have the right failure reason");
    },
  });

  for (const { url, color, width, height } of [
    {
      url: BASE_URL + "test/sample_image_red_1920x1080.jpg",
      color: [255, 0, 0],
      width: 1920,
      height: 1080,
    },
    {
      url: BASE_URL + "test/sample_image_green_1024x1024.jpg",
      color: [0, 255, 0],
      width: 1024,
      height: 1024,
    },
    {
      url: BASE_URL + "test/sample_image_blue_300x600.jpg",
      color: [0, 0, 255],
      width: 300,
      height: 600,
    },
  ]) {
    dontExpireThumbnailURLs([url]);
    const capturedPromise = bgAddPageThumbObserver(url);
    await bgCapture(url);
    await capturedPromise;
    ok(thumbnailExists(url), "The image thumbnail should exist after capture");

    const thumb = PageThumbs.getThumbnailURL(url);
    const htmlns = "http://www.w3.org/1999/xhtml";
    const img = document.createElementNS(htmlns, "img");
    img.src = thumb;
    await BrowserTestUtils.waitForEvent(img, "load");

    // 448px is the default max-width of an image thumbnail
    const expectedWidth = Math.min(448, width);
    // Tall images are clipped to {width}x{width}
    const expectedHeight = Math.min(
      (expectedWidth * height) / width,
      expectedWidth
    );
    // Fuzzy equality to account for rounding
    ok(
      Math.abs(img.naturalWidth - expectedWidth) <= 1,
      "The thumbnail should have the right width"
    );
    ok(
      Math.abs(img.naturalHeight - expectedHeight) <= 1,
      "The thumbnail should have the right height"
    );

    // Draw the image to a canvas and compare the pixel color values.
    const canvas = document.createElementNS(htmlns, "canvas");
    canvas.width = expectedWidth;
    canvas.height = expectedHeight;
    const ctx = canvas.getContext("2d");
    ctx.drawImage(img, 0, 0, expectedWidth, expectedHeight);
    const [r, g, b] = ctx.getImageData(
      0,
      0,
      expectedWidth,
      expectedHeight
    ).data;
    // Fuzzy equality to account for image encoding
    ok(
      Math.abs(r - color[0]) <= 2 &&
        Math.abs(g - color[1]) <= 2 &&
        Math.abs(b - color[2]) <= 2,
      "The thumbnail should have the right color"
    );

    removeThumbnail(url);
  }
});
