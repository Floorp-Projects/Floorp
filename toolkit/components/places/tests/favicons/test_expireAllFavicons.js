/**
 * This file tests that favicons are correctly expired by expireAllFavicons.
 */

"use strict";

const TEST_PAGE_URI = NetUtil.newURI("http://example.com/");
const BOOKMARKED_PAGE_URI = NetUtil.newURI("http://example.com/bookmarked");

add_task(function* test_expireAllFavicons() {
  const {FAVICON_LOAD_NON_PRIVATE} = PlacesUtils.favicons;

  // Add a visited page.
  yield PlacesTestUtils.addVisits({ uri: TEST_PAGE_URI, transition: TRANSITION_TYPED });

  // Set a favicon for our test page.
  yield promiseSetIconForPage(TEST_PAGE_URI, SMALLPNG_DATA_URI);

  // Add a page with a bookmark.
  yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: BOOKMARKED_PAGE_URI,
    title: "Test bookmark"
  });

  // Set a favicon for our bookmark.
  yield promiseSetIconForPage(BOOKMARKED_PAGE_URI, SMALLPNG_DATA_URI);

  // Start expiration only after data has been saved in the database.
  let promise = promiseTopicObserved(PlacesUtils.TOPIC_FAVICONS_EXPIRED);
  PlacesUtils.favicons.expireAllFavicons();
  yield promise;

  // Check that the favicons for the pages we added were removed.
  yield promiseFaviconMissingForPage(TEST_PAGE_URI);
  yield promiseFaviconMissingForPage(BOOKMARKED_PAGE_URI);
});

function run_test() {
  run_next_test();
}
