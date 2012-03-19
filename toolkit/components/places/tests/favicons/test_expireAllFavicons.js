/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests that favicons are correctly expired by expireAllFavicons.
 */

////////////////////////////////////////////////////////////////////////////////
/// Globals

XPCOMUtils.defineLazyServiceGetter(this, "gHistory",
                                   "@mozilla.org/browser/history;1",
                                   "mozIAsyncHistory");

const TEST_PAGE_URI = NetUtil.newURI("http://example.com/");
const BOOKMARKED_PAGE_URI = NetUtil.newURI("http://example.com/bookmarked");

////////////////////////////////////////////////////////////////////////////////
/// Tests

function run_test() {
  run_next_test();
}

add_test(function test_expireAllFavicons() {
  // Set up an observer to wait for favicons expiration to finish.
  Services.obs.addObserver(function EAF_observer(aSubject, aTopic, aData) {
    Services.obs.removeObserver(EAF_observer, aTopic);

    // Check that the favicons for the pages we added were removed.
    checkFaviconMissingForPage(TEST_PAGE_URI, function () {
      checkFaviconMissingForPage(BOOKMARKED_PAGE_URI, function () {
        run_next_test();
      });
    });
  }, PlacesUtils.TOPIC_FAVICONS_EXPIRED, false);

  // Add a visited page.
  gHistory.updatePlaces({
    uri: TEST_PAGE_URI,
    visits: [{
      transitionType: Ci.nsINavHistoryService.TRANSITION_TYPED,
      visitDate: Date.now() * 1000
    }]
  }, {
    handleError: function EAF_handleError(aResultCode, aPlaceInfo) {
      do_throw("Unexpected error: " + aResultCode);
    },
    handleResult: function EAF_handleResult(aPlaceInfo) {
      PlacesUtils.favicons.setAndFetchFaviconForPage(TEST_PAGE_URI,
                                                     SMALLPNG_DATA_URI, true);

      // Add a page with a bookmark.
      PlacesUtils.bookmarks.insertBookmark(
                            PlacesUtils.toolbarFolderId, BOOKMARKED_PAGE_URI,
                            PlacesUtils.bookmarks.DEFAULT_INDEX,
                            "Test bookmark");
      PlacesUtils.favicons.setAndFetchFaviconForPage(
        BOOKMARKED_PAGE_URI, SMALLPNG_DATA_URI, true,
        function EAF_onFaviconDataAvailable() {
          // Start expiration only after data has been saved in the database.
          PlacesUtils.favicons.expireAllFavicons();
        });
    }
  });
});
