/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* runTests() {
  let url1 = "http://example.com/1";
  ok(!thumbnailExists(url1), "First file should not exist yet.");

  let url2 = "http://example.com/2";
  ok(!thumbnailExists(url2), "Second file should not exist yet.");

  let defaultTimeout = BackgroundPageThumbs._destroyBrowserTimeout;
  BackgroundPageThumbs._destroyBrowserTimeout = 1000;

  yield bgCapture(url1);
  ok(thumbnailExists(url1), "First file should exist after capture.");
  removeThumbnail(url1);

  yield wait(2000);
  is(BackgroundPageThumbs._thumbBrowser, undefined,
     "Thumb browser should be destroyed after timeout.");
  BackgroundPageThumbs._destroyBrowserTimeout = defaultTimeout;

  yield bgCapture(url2);
  ok(thumbnailExists(url2), "Second file should exist after capture.");
  removeThumbnail(url2);

  isnot(BackgroundPageThumbs._thumbBrowser, undefined,
        "Thumb browser should exist immediately after capture.");
}
