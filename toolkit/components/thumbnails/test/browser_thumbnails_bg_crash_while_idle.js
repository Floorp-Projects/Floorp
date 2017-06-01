/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* runTests() {
  // make a good capture first - this ensures we have the <browser>
  let goodUrl = bgTestPageURL();
  yield bgCapture(goodUrl);
  ok(thumbnailExists(goodUrl), "Thumbnail should be cached after capture");
  removeThumbnail(goodUrl);

  // Nothing is pending - crash the process.
  info("Crashing the thumbnail content process.");
  let crash = yield BrowserTestUtils.crashBrowser(BackgroundPageThumbs._thumbBrowser, false);
  ok(crash.CrashTime, "Saw a crash from this test");

  // Now queue another capture and ensure it recovers.
  bgCapture(goodUrl, { onDone: () => {
    ok(thumbnailExists(goodUrl), "We should have recovered and handled new capture requests");
    removeThumbnail(goodUrl);
    // Test done.
    next();
  }});

  yield true;
}
