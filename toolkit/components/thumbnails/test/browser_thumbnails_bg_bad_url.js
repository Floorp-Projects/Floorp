/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let url = "invalid-protocol://ffggfsdfsdf/";
  ok(!thumbnailExists(url), "Thumbnail should not be cached already.");
  let numCalls = 0;
  BackgroundPageThumbs.capture(url, {
    onDone: function onDone(capturedURL) {
      is(capturedURL, url, "Captured URL should be URL passed to capture");
      is(numCalls++, 0, "onDone should be called only once");
      ok(!thumbnailExists(url),
         "Capture failed so thumbnail should not be cached");
      next();
    },
  });
  yield true;
}
