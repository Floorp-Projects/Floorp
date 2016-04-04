/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* runTests() {
  let url = "http://www.example.com/";
  ok(!thumbnailExists(url), "Thumbnail should not be cached yet.");

  let capturePromise = new Promise(resolve => {
    bgAddPageThumbObserver(url).then(() => {
      ok(true, `page-thumbnail created for ${url}`);
      resolve();
    });
  });
  let capturedURL = yield bgCapture(url);
  is(capturedURL, url, "Captured URL should be URL passed to capture");
  yield capturePromise;

  ok(thumbnailExists(url), "Thumbnail should be cached after capture");
  removeThumbnail(url);
}
