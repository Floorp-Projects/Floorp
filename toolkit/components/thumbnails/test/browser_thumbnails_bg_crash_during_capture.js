/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bg_crash_during_capture() {
  // make a good capture first - this ensures we have the <browser>
  let goodUrl = bgTestPageURL();
  await bgCapture(goodUrl);
  ok(thumbnailExists(goodUrl), "Thumbnail should be cached after capture");
  removeThumbnail(goodUrl);

  // queue up 2 captures - the first has a wait, so this is the one that
  // will die.  The second one should immediately capture after the crash.
  let waitUrl = bgTestPageURL({ wait: 30000 });
  let sawWaitUrlCapture = false;
  let failCapture = bgCapture(waitUrl, {
    onDone: () => {
      sawWaitUrlCapture = true;
      ok(
        !thumbnailExists(waitUrl),
        "Thumbnail should not have been saved due to the crash"
      );
    },
  });
  let goodCapture = bgCapture(goodUrl, {
    onDone: () => {
      ok(sawWaitUrlCapture, "waitUrl capture should have finished first");
      ok(
        thumbnailExists(goodUrl),
        "We should have recovered and completed the 2nd capture after the crash"
      );
      removeThumbnail(goodUrl);
    },
  });

  let crashPromise = bgAddPageThumbObserver(waitUrl).catch(err => {
    ok(/page-thumbnail:error/.test(err), "Got the right kind of error");
  });
  let capturePromise = bgAddPageThumbObserver(goodUrl);

  info("Crashing the thumbnail content process.");
  let crash = await BrowserTestUtils.crashFrame(
    BackgroundPageThumbs._thumbBrowser,
    false
  );
  ok(crash.CrashTime, "Saw a crash from this test");

  await crashPromise;
  await Promise.all([failCapture, goodCapture, capturePromise]);
});
