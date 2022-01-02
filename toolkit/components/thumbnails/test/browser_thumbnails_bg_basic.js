/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bg_basic() {
  let url = "http://www.example.com/";
  ok(!thumbnailExists(url), "Thumbnail should not be cached yet.");

  let capturePromise = bgAddPageThumbObserver(url);
  let [capturedURL] = await bgCapture(url);
  is(capturedURL, url, "Captured URL should be URL passed to capture");
  await capturePromise;

  ok(thumbnailExists(url), "Thumbnail should be cached after capture");
  removeThumbnail(url);
});
