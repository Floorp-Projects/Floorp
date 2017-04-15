/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* runTests() {
  let crashObserver = bgAddCrashObserver();

  // make a good capture first - this ensures we have the <browser>
  let goodUrl = bgTestPageURL();
  yield bgCapture(goodUrl);
  ok(thumbnailExists(goodUrl), "Thumbnail should be cached after capture");
  removeThumbnail(goodUrl);

  // inject our content script.
  let mm = bgInjectCrashContentScript();

  // the observer for the crashing process is basically async, so it's hard
  // to know when the <browser> has actually seen it.  Easist is to just add
  // our own observer.
  Services.obs.addObserver(function onCrash() {
    Services.obs.removeObserver(onCrash, "oop-frameloader-crashed");
    // spin the event loop to ensure the BPT observer was called.
    executeSoon(function() {
      // Now queue another capture and ensure it recovers.
      bgCapture(goodUrl, { onDone: () => {
        ok(thumbnailExists(goodUrl), "We should have recovered and handled new capture requests");
        removeThumbnail(goodUrl);
        // Test done.
        ok(crashObserver.crashed, "Saw a crash from this test");
        next();
      }});
    });
  }, "oop-frameloader-crashed");

  // Nothing is pending - crash the process.
  info("Crashing the thumbnail content process.");
  mm.sendAsyncMessage("thumbnails-test:crash");
  yield true;
}
