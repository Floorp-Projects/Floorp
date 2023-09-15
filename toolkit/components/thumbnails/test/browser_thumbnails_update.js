/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests check the auto-update facility of the thumbnail service.
 */

function ensureThumbnailStale(url) {
  // We go behind the back of the thumbnail service and change the
  // mtime of the file to be in the past.
  let fname = PageThumbsStorageService.getFilePathForURL(url);
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(fname);
  ok(file.exists(), fname + " should exist");
  // Set it as very stale...
  file.lastModifiedTime = Date.now() - 1000000000;
}

function getThumbnailModifiedTime(url) {
  let fname = PageThumbsStorageService.getFilePathForURL(url);
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(fname);
  return file.lastModifiedTime;
}

/**
 * Check functionality of a normal captureAndStoreIfStale request
 */
add_task(async function thumbnails_captureAndStoreIfStale_normal() {
  const URL =
    "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?simple";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      let numNotifications = 0;

      let observed = TestUtils.topicObserved(
        "page-thumbnail:create",
        (subject, data) => {
          is(data, URL, "data is our test URL");

          // Once we get the second notification, we saw the last captureAndStoreIsStale,
          // and we can tear down.
          if (++numNotifications == 2) {
            return true;
          }

          return false;
        }
      );
      await PageThumbs.captureAndStore(browser);
      // We've got a capture so should have seen the observer.
      is(numNotifications, 1, "got notification of item being created.");

      await PageThumbs.captureAndStoreIfStale(browser);
      is(
        numNotifications,
        1,
        "still only 1 notification of item being created."
      );

      ensureThumbnailStale(URL);
      await PageThumbs.captureAndStoreIfStale(browser);
      await observed;
    }
  );
});

/**
 * Check functionality of captureAndStoreIfStale when there is an error response
 * from the server.
 */
add_task(async function thumbnails_captureAndStoreIfStale_error_response() {
  const URL =
    "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?fail";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      await captureAndCheckColor(0, 255, 0, "we have a green thumbnail");

      // update the thumbnail to be stale, then re-request it.  The server will
      // return a 400 response and a red thumbnail.
      // The service should not save the thumbnail - so we (a) check the thumbnail
      // remains green and (b) check the mtime of the file is < now.
      ensureThumbnailStale(URL);
      BrowserTestUtils.startLoadingURIString(browser, URL);
      await BrowserTestUtils.browserLoaded(browser);

      // now() returns a higher-precision value than the modified time of a file.
      // As we set the thumbnail very stale, allowing 1 second of "slop" here
      // works around this while still keeping the test valid.
      let now = Date.now() - 1000;
      await PageThumbs.captureAndStoreIfStale(gBrowser.selectedBrowser);

      ok(getThumbnailModifiedTime(URL) < now, "modified time should be < now");
      let [r, g, b] = await retrieveImageDataForURL(URL);
      is("" + [r, g, b], "" + [0, 255, 0], "thumbnail is still green");
    }
  );
});

/**
 * Check functionality of captureAndStoreIfStale when there is a non-error
 * response from the server.  This test is somewhat redundant - although it is
 * using a http:// URL instead of a data: url like most others.
 */
add_task(async function thumbnails_captureAndStoreIfStale_non_error_response() {
  const URL =
    "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?ok";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      await captureAndCheckColor(0, 255, 0, "we have a green thumbnail");
      // update the thumbnail to be stale, then re-request it.  The server will
      // return a 200 response and a red thumbnail - so that new thumbnail should
      // end up captured.
      ensureThumbnailStale(URL);
      BrowserTestUtils.startLoadingURIString(browser, URL);
      await BrowserTestUtils.browserLoaded(browser);

      // now() returns a higher-precision value than the modified time of a file.
      // As we set the thumbnail very stale, allowing 1 second of "slop" here
      // works around this while still keeping the test valid.
      let now = Date.now() - 1000;
      await PageThumbs.captureAndStoreIfStale(browser);
      Assert.greater(
        getThumbnailModifiedTime(URL),
        now,
        "modified time should be >= now"
      );
      let [r, g, b] = await retrieveImageDataForURL(URL);
      is("" + [r, g, b], "" + [255, 0, 0], "thumbnail is now red");
    }
  );
});

/**
 * Check functionality of captureAndStore when there is an error response
 * from the server.
 */
add_task(async function thumbnails_captureAndStore_error_response() {
  const URL =
    "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?fail";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      await captureAndCheckColor(0, 255, 0, "we have a green thumbnail");
    }
  );

  // do it again - the server will return a 400, so the foreground service
  // should not update it.

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      await captureAndCheckColor(0, 255, 0, "we still have a green thumbnail");
    }
  );
});

/**
 * Check functionality of captureAndStore when there is an OK response
 * from the server.
 */
add_task(async function thumbnails_captureAndStore_ok_response() {
  const URL =
    "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_update.sjs?ok";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      await captureAndCheckColor(0, 255, 0, "we have a green thumbnail");
    }
  );

  // do it again - the server will return a 200, so the foreground service
  // should  update it.

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      await captureAndCheckColor(255, 0, 0, "we now have a red thumbnail");
    }
  );
});
