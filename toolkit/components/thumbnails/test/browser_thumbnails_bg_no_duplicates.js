/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bg_no_duplicates() {
  let url = "http://example.com/1";
  ok(!thumbnailExists(url), "Thumbnail file should not already exist.");

  let firstCapture = bgCapture(url, {
    onDone: doneUrl => {
      is(doneUrl, url, "called back with correct url");
      ok(thumbnailExists(url), "Thumbnail file should now exist.");
      removeThumbnail(url);
    },
  });

  let secondCapture = bgCapture(url, {
    onDone: doneUrl => {
      is(doneUrl, url, "called back with correct url");
      ok(!thumbnailExists(url), "Thumbnail file should still be deleted.");
    },
  });

  await Promise.all([firstCapture, secondCapture]);
});
