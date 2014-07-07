/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let numNotifications = 0;
  function observe(subject, topic, data) {
    is(topic, "page-thumbnail:create", "got expected topic");
    numNotifications += 1;
  }

  Services.obs.addObserver(observe, "page-thumbnail:create", false);

  let url = "http://example.com/";
  let file = thumbnailFile(url);
  ok(!file.exists(), "Thumbnail file should not already exist.");

  let capturedURL = yield bgCaptureIfMissing(url);
  is(numNotifications, 1, "got notification of item being created.");
  is(capturedURL, url, "Captured URL should be URL passed to capture");
  ok(file.exists(url), "Thumbnail should be cached after capture");

  let past = Date.now() - 1000000000;
  let pastFudge = past + 30000;
  file.lastModifiedTime = past;
  ok(file.lastModifiedTime < pastFudge, "Last modified time should stick!");
  capturedURL = yield bgCaptureIfMissing(url);
  is(numNotifications, 1, "still only 1 notification of item being created.");
  is(capturedURL, url, "Captured URL should be URL passed to second capture");
  ok(file.exists(), "Thumbnail should remain cached after second capture");
  ok(file.lastModifiedTime < pastFudge,
     "File should not have been overwritten");

  file.remove(false);
  Services.obs.removeObserver(observe, "page-thumbnail:create");
}
