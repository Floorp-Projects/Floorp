/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let url = "http://example.com/";
  let file = thumbnailFile(url);
  ok(!file.exists(), "Thumbnail file should not already exist.");

  let capturedURL = yield bgCaptureIfMissing(url);
  is(capturedURL, url, "Captured URL should be URL passed to capture");
  ok(file.exists(url), "Thumbnail should be cached after capture");

  let past = Date.now() - 1000000000;
  let pastFudge = past + 30000;
  file.lastModifiedTime = past;
  ok(file.lastModifiedTime < pastFudge, "Last modified time should stick!");
  capturedURL = yield bgCaptureIfMissing(url);
  is(capturedURL, url, "Captured URL should be URL passed to second capture");
  ok(file.exists(), "Thumbnail should remain cached after second capture");
  ok(file.lastModifiedTime < pastFudge,
     "File should not have been overwritten");

  file.remove(false);
}
