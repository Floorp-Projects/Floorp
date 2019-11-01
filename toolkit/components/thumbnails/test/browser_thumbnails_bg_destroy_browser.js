/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bg_destroy_browser() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount", 1]],
  });

  let url1 = "http://example.com/1";
  ok(!thumbnailExists(url1), "First file should not exist yet.");

  let url2 = "http://example.com/2";
  ok(!thumbnailExists(url2), "Second file should not exist yet.");

  let defaultTimeout = BackgroundPageThumbs._destroyBrowserTimeout;
  BackgroundPageThumbs._destroyBrowserTimeout = 1000;

  await bgCapture(url1);
  ok(thumbnailExists(url1), "First file should exist after capture.");
  removeThumbnail(url1);

  // arbitrary wait - intermittent failures noted after 2 seconds
  await TestUtils.waitForCondition(
    () => {
      return BackgroundPageThumbs._thumbBrowser === undefined;
    },
    "BackgroundPageThumbs._thumbBrowser should eventually be discarded.",
    1000,
    5
  );

  is(
    BackgroundPageThumbs._thumbBrowser,
    undefined,
    "Thumb browser should be destroyed after timeout."
  );
  BackgroundPageThumbs._destroyBrowserTimeout = defaultTimeout;

  await bgCapture(url2);
  ok(thumbnailExists(url2), "Second file should exist after capture.");
  removeThumbnail(url2);

  isnot(
    BackgroundPageThumbs._thumbBrowser,
    undefined,
    "Thumb browser should exist immediately after capture."
  );
});
