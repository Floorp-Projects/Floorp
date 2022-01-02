/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bg_redirect() {
  let finalURL = "https://example.com/redirected";
  let originalURL = bgTestPageURL({ redirect: finalURL });

  ok(
    !thumbnailExists(originalURL),
    "Thumbnail file for original URL should not exist yet."
  );
  ok(
    !thumbnailExists(finalURL),
    "Thumbnail file for final URL should not exist yet."
  );

  let captureOriginalPromise = bgAddPageThumbObserver(originalURL);
  let captureFinalPromise = bgAddPageThumbObserver(finalURL);

  let [capturedURL] = await bgCapture(originalURL);
  is(capturedURL, originalURL, "Captured URL should be URL passed to capture");
  await captureOriginalPromise;
  await captureFinalPromise;
  ok(
    thumbnailExists(originalURL),
    "Thumbnail for original URL should be cached"
  );
  ok(thumbnailExists(finalURL), "Thumbnail for final URL should be cached");

  removeThumbnail(originalURL);
  removeThumbnail(finalURL);
});
