/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// the following tests attempt to display modal dialogs.  The test just
// relies on the fact that if the dialog was displayed the test will hang
// and timeout.  IOW - the tests would pass if the dialogs appear and are
// manually closed by the user - so don't do that :)  (obviously there is
// noone available to do that when run via tbpl etc, so this should be safe,
// and it's tricky to use the window-watcher to check a window *does not*
// appear - how long should the watcher be active before assuming it's not
// going to appear?)
function* runTests() {
  let url = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/authenticate.sjs?user=anyone";
  ok(!thumbnailExists(url), "Thumbnail file should not already exist.");

  let capturedURL = yield bgCapture(url);
  is(capturedURL, url, "Captured URL should be URL passed to capture.");
  ok(thumbnailExists(url),
     "Thumbnail file should exist even though it requires auth.");
  removeThumbnail(url);
}
