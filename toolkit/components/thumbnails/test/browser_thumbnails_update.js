/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests check the auto-update facility of the thumbnail service.
 */

function runTests() {
  // A "trampoline" - a generator that iterates over sub-iterators
  let tests = [
    simpleCaptureTest,
    capIfStaleErrorResponseUpdateTest,
    capIfStaleGoodResponseUpdateTest,
    regularCapErrorResponseUpdateTest,
    regularCapGoodResponseUpdateTest
  ];
  for (let test of tests) {
    info("Running subtest " + test.name);
    for (let iterator of test())
      yield iterator;
  }
}

function ensureThumbnailStale(url) {
  // We go behind the back of the thumbnail service and change the
  // mtime of the file to be in the past.
  let fname = PageThumbsStorage.getFilePathForURL(url);
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(fname);
  ok(file.exists(), fname + " should exist");
  // Set it as very stale...
  file.lastModifiedTime = Date.now() - 1000000000;
}

function getThumbnailModifiedTime(url) {
  let fname = PageThumbsStorage.getFilePathForURL(url);
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(fname);
  return file.lastModifiedTime;
}

// The tests!
/* Check functionality of a normal captureAndStoreIfStale request */
function simpleCaptureTest() {
  let numNotifications = 0;
  const URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?simple";

  function observe(subject, topic, data) {
    is(topic, "page-thumbnail:create", "got expected topic");
    is(data, URL, "data is our test URL");
    if (++numNotifications == 2) {
      // This is the final notification and signals test success...
      Services.obs.removeObserver(observe, "page-thumbnail:create");
      gBrowser.removeTab(gBrowser.selectedTab);
      next();
    }
  }

  Services.obs.addObserver(observe, "page-thumbnail:create", false);
  // Create a tab - we don't care what the content is.
  yield addTab(URL);
  let browser = gBrowser.selectedBrowser;

  // Capture the screenshot.
  PageThumbs.captureAndStore(browser, function () {
    // We've got a capture so should have seen the observer.
    is(numNotifications, 1, "got notification of item being created.");
    // The capture is now "fresh" - so requesting the URL should not cause
    // a new capture.
    PageThumbs.captureAndStoreIfStale(browser);
    is(numNotifications, 1, "still only 1 notification of item being created.");

    ensureThumbnailStale(URL);
    // Ask for it to be updated.
    PageThumbs.captureAndStoreIfStale(browser);
    // But it's async, so wait - our observer above will call next() when
    // the notification comes.
  });
  yield undefined // wait for callbacks to call 'next'...
}

/* Check functionality of captureAndStoreIfStale when there is an error response
   from the server.
 */
function capIfStaleErrorResponseUpdateTest() {
  const URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?fail";
  yield addTab(URL);

  yield captureAndCheckColor(0, 255, 0, "we have a green thumbnail");
  // update the thumbnail to be stale, then re-request it.  The server will
  // return a 400 response and a red thumbnail.
  // The service should not save the thumbnail - so we (a) check the thumbnail
  // remains green and (b) check the mtime of the file is < now.
  ensureThumbnailStale(URL);
  yield navigateTo(URL);
  // now() returns a higher-precision value than the modified time of a file.
  // As we set the thumbnail very stale, allowing 1 second of "slop" here
  // works around this while still keeping the test valid.
  let now = Date.now() - 1000 ;
  PageThumbs.captureAndStoreIfStale(gBrowser.selectedBrowser, () => {
    ok(getThumbnailModifiedTime(URL) < now, "modified time should be < now");
    retrieveImageDataForURL(URL, function ([r, g, b]) {
      is("" + [r,g,b], "" + [0, 255, 0], "thumbnail is still green");
      gBrowser.removeTab(gBrowser.selectedTab);
      next();
    });
  });
  yield undefined; // wait for callback to call 'next'...
}

/* Check functionality of captureAndStoreIfStale when there is a non-error
   response from the server.  This test is somewhat redundant - although it is
   using a http:// URL instead of a data: url like most others.
 */
function capIfStaleGoodResponseUpdateTest() {
  const URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?ok";
  yield addTab(URL);
  let browser = gBrowser.selectedBrowser;

  yield captureAndCheckColor(0, 255, 0, "we have a green thumbnail");
  // update the thumbnail to be stale, then re-request it.  The server will
  // return a 200 response and a red thumbnail - so that new thumbnail should
  // end up captured.
  ensureThumbnailStale(URL);
  yield navigateTo(URL);
  // now() returns a higher-precision value than the modified time of a file.
  // As we set the thumbnail very stale, allowing 1 second of "slop" here
  // works around this while still keeping the test valid.
  let now = Date.now() - 1000 ;
  PageThumbs.captureAndStoreIfStale(browser, () => {
    ok(getThumbnailModifiedTime(URL) >= now, "modified time should be >= now");
    // the captureAndStoreIfStale request saw a 200 response with the red body,
    // so we expect to see the red version here.
    retrieveImageDataForURL(URL, function ([r, g, b]) {
      is("" + [r,g,b], "" + [255, 0, 0], "thumbnail is now red");
      next();
    });
  });
  yield undefined; // wait for callback to call 'next'...
}

/* Check functionality of captureAndStore when there is an error response
   from the server.
 */
function regularCapErrorResponseUpdateTest() {
  const URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?fail";
  yield addTab(URL);
  yield captureAndCheckColor(0, 255, 0, "we have a green thumbnail");
  gBrowser.removeTab(gBrowser.selectedTab);
  // do it again - the server will return a 400, so the foreground service
  // should not update it.
  yield addTab(URL);
  yield captureAndCheckColor(0, 255, 0, "we still have a green thumbnail");
}

/* Check functionality of captureAndStore when there is an OK response
   from the server.
 */
function regularCapGoodResponseUpdateTest() {
  const URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?ok";
  yield addTab(URL);
  yield captureAndCheckColor(0, 255, 0, "we have a green thumbnail");
  gBrowser.removeTab(gBrowser.selectedTab);
  // do it again - the server will return a 200, so the foreground service
  // should  update it.
  yield addTab(URL);
  yield captureAndCheckColor(255, 0, 0, "we now  have a red thumbnail");
}
