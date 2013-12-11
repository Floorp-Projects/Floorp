/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let url = "http://example.com/1";
  ok(!thumbnailExists(url), "Thumbnail file should not already exist.");
  let numCallbacks = 0;
  let doneCallback = function(doneUrl) {
    is(doneUrl, url, "called back with correct url");
    numCallbacks += 1;
    // We will delete the file after the first callback, then check it
    // still doesn't exist on the second callback, which should give us
    // confidence that we didn't end up with 2 different captures happening
    // for the same url...
    if (numCallbacks == 1) {
      ok(thumbnailExists(url), "Thumbnail file should now exist.");
      removeThumbnail(url);
      return;
    }
    if (numCallbacks == 2) {
      ok(!thumbnailExists(url), "Thumbnail file should still be deleted.");
      // and that's all we expect, so we are done...
      next();
      return;
    }
    ok(false, "only expecting 2 callbacks");
  }
  BackgroundPageThumbs.capture(url, {onDone: doneCallback});
  BackgroundPageThumbs.capture(url, {onDone: doneCallback});
  yield true;
}
