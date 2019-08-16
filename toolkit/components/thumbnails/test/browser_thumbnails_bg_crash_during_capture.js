/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* runTests() {
  // make a good capture first - this ensures we have the <browser>
  let goodUrl = bgTestPageURL();
  yield bgCapture(goodUrl);
  ok(thumbnailExists(goodUrl), "Thumbnail should be cached after capture");
  removeThumbnail(goodUrl);

  // queue up 2 captures - the first has a wait, so this is the one that
  // will die.  The second one should immediately capture after the crash.
  let waitUrl = bgTestPageURL({ wait: 30000 });
  let sawWaitUrlCapture = false;
  bgCapture(waitUrl, {
    onDone: () => {
      sawWaitUrlCapture = true;
      ok(
        !thumbnailExists(waitUrl),
        "Thumbnail should not have been saved due to the crash"
      );
    },
  });
  bgCapture(goodUrl, {
    onDone: () => {
      ok(sawWaitUrlCapture, "waitUrl capture should have finished first");
      ok(
        thumbnailExists(goodUrl),
        "We should have recovered and completed the 2nd capture after the crash"
      );
      removeThumbnail(goodUrl);
      // Test done.
      next();
    },
  });
  let crashPromise = new Promise(resolve => {
    bgAddPageThumbObserver(waitUrl).catch(function(err) {
      ok(true, `page-thumbnail error thrown for ${waitUrl}`);
      resolve();
    });
  });
  let capturePromise = new Promise(resolve => {
    bgAddPageThumbObserver(goodUrl).then(() => {
      ok(true, `page-thumbnail created for ${goodUrl}`);
      resolve();
    });
  });

  info("Crashing the thumbnail content process.");
  let crash = yield BrowserTestUtils.crashFrame(
    BackgroundPageThumbs._thumbBrowser,
    false
  );
  ok(crash.CrashTime, "Saw a crash from this test");

  yield crashPromise;
  yield capturePromise;
}
