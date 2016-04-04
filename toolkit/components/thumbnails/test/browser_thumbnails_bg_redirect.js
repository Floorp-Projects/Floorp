/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* runTests() {
  let finalURL = "http://example.com/redirected";
  let originalURL = bgTestPageURL({ redirect: finalURL });

  ok(!thumbnailExists(originalURL),
     "Thumbnail file for original URL should not exist yet.");
  ok(!thumbnailExists(finalURL),
     "Thumbnail file for final URL should not exist yet.");

  let captureOriginalPromise = new Promise(resolve => {
    bgAddPageThumbObserver(originalURL).then(() => {
      ok(true, `page-thumbnail created for ${originalURL}`);
      resolve();
    });
  });

  let captureFinalPromise = new Promise(resolve => {
    bgAddPageThumbObserver(finalURL).then(() => {
      ok(true, `page-thumbnail created for ${finalURL}`);
      resolve();
    });
  });

  let capturedURL = yield bgCapture(originalURL);
  is(capturedURL, originalURL,
     "Captured URL should be URL passed to capture");
  yield captureOriginalPromise;
  yield captureFinalPromise;
  ok(thumbnailExists(originalURL),
     "Thumbnail for original URL should be cached");
  ok(thumbnailExists(finalURL),
     "Thumbnail for final URL should be cached");

  removeThumbnail(originalURL);
  removeThumbnail(finalURL);
}
