/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let crashObserver = bgAddCrashObserver();

  // make a good capture first - this ensures we have the <browser>
  let goodUrl = bgTestPageURL();
  yield bgCapture(goodUrl);
  ok(thumbnailExists(goodUrl), "Thumbnail should be cached after capture");
  removeThumbnail(goodUrl);

  // inject our content script.
  let mm = bgInjectCrashContentScript();

  // queue up 2 captures - the first has a wait, so this is the one that
  // will die.  The second one should immediately capture after the crash.
  let waitUrl = bgTestPageURL({ wait: 30000 });
  let sawWaitUrlCapture = false;
  bgCapture(waitUrl, { onDone: () => {
    sawWaitUrlCapture = true;
    ok(!thumbnailExists(waitUrl), "Thumbnail should not have been saved due to the crash");
  }});
  bgCapture(goodUrl, { onDone: () => {
    ok(sawWaitUrlCapture, "waitUrl capture should have finished first");
    ok(thumbnailExists(goodUrl), "We should have recovered and completed the 2nd capture after the crash");
    removeThumbnail(goodUrl);
    // Test done.
    ok(crashObserver.crashed, "Saw a crash from this test");
    next();
  }});

  info("Crashing the thumbnail content process.");
  mm.sendAsyncMessage("thumbnails-test:crash");
  yield true;
}
