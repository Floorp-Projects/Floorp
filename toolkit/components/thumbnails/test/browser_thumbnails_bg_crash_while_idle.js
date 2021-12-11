/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bg_crash_while_idle() {
  // make a good capture first - this ensures we have the <browser>
  let goodUrl = bgTestPageURL();
  await bgCapture(goodUrl);
  ok(thumbnailExists(goodUrl), "Thumbnail should be cached after capture");
  removeThumbnail(goodUrl);

  // Nothing is pending - crash the process.
  info("Crashing the thumbnail content process.");
  let crash = await BrowserTestUtils.crashFrame(
    BackgroundPageThumbs._thumbBrowser,
    false
  );
  ok(crash.CrashTime, "Saw a crash from this test");

  // Now queue another capture and ensure it recovers.
  await bgCapture(goodUrl, {
    onDone: () => {
      ok(
        thumbnailExists(goodUrl),
        "We should have recovered and handled new capture requests"
      );
      removeThumbnail(goodUrl);
    },
  });
});
