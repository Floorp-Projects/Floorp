/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests a zero frecency is correctly updated when inserting new valid visits.

function run_test() {
  run_next_test()
}

add_task(function* () {
  const TEST_URI = NetUtil.newURI("http://example.com/");
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                TEST_URI,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "A title");
  yield PlacesTestUtils.promiseAsyncUpdates();
  do_check_true(frecencyForUrl(TEST_URI) > 0);

  // Removing the bookmark should leave an orphan page with zero frecency.
  // Note this would usually be expired later by expiration.
  PlacesUtils.bookmarks.removeItem(id);
  yield PlacesTestUtils.promiseAsyncUpdates();
  do_check_eq(frecencyForUrl(TEST_URI), 0);

  // Now add a valid visit to the page, frecency should increase.
  yield PlacesTestUtils.addVisits({ uri: TEST_URI });
  do_check_true(frecencyForUrl(TEST_URI) > 0);
});
