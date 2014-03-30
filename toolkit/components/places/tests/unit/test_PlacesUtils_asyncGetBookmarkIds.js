/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests PlacesUtils.asyncGetBookmarkIds method.
 */

const TEST_URL = "http://www.example.com/";

[

  function test_no_bookmark() {
    PlacesUtils.asyncGetBookmarkIds(TEST_URL, (aItemIds, aURI) => {
      do_check_eq(aItemIds.length, 0);
      do_check_eq(aURI, TEST_URL);
      run_next_test();
    });
  },

  function test_one_bookmark_nsIURI() {
    let uri = NetUtil.newURI(TEST_URL);
    let itemId = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.unfiledBookmarksFolderId, uri, "test",
      PlacesUtils.bookmarks.DEFAULT_INDEX
    );
    PlacesUtils.asyncGetBookmarkIds(uri, (aItemIds, aURI) => {
      do_check_eq(aItemIds.length, 1);
      do_check_eq(aItemIds[0], itemId);
      do_check_true(aURI.equals(uri));
      PlacesUtils.bookmarks.removeItem(itemId);
      run_next_test();
    });
  },

  function test_one_bookmark_spec() {
    let uri = NetUtil.newURI(TEST_URL);
    let itemId = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.unfiledBookmarksFolderId, uri, "test",
      PlacesUtils.bookmarks.DEFAULT_INDEX
    );
    PlacesUtils.asyncGetBookmarkIds(TEST_URL, (aItemIds, aURI) => {
      do_check_eq(aItemIds.length, 1);
      do_check_eq(aItemIds[0], itemId);
      do_check_eq(aURI, TEST_URL);
      PlacesUtils.bookmarks.removeItem(itemId);
      run_next_test();
    });
  },

  function test_multiple_bookmarks() {
    let uri = NetUtil.newURI(TEST_URL);
    let itemIds = [];
    itemIds.push(PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.unfiledBookmarksFolderId, uri, "test",
      PlacesUtils.bookmarks.DEFAULT_INDEX
    ));
    itemIds.push(PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.unfiledBookmarksFolderId, uri, "test",
      PlacesUtils.bookmarks.DEFAULT_INDEX
    ));
    PlacesUtils.asyncGetBookmarkIds(uri, (aItemIds, aURI) => {
      do_check_eq(aItemIds.length, 2);
      do_check_true(do_compare_arrays(itemIds, aItemIds));
      do_check_true(aURI.equals(uri));
      itemIds.forEach(PlacesUtils.bookmarks.removeItem);
      run_next_test();
    });
  },

  function test_cancel() {
    let pending = PlacesUtils.asyncGetBookmarkIds(TEST_URL, (aItemIds, aURI) => {
      do_throw("A canceled pending statement should not be invoked");
    });
    pending.cancel();
    PlacesUtils.asyncGetBookmarkIds(TEST_URL, (aItemIds, aURI) => {
      do_check_eq(aItemIds.length, 0);
      do_check_eq(aURI, TEST_URL);
      run_next_test();
    });
  },

].forEach(add_test);

function run_test() {
  run_next_test();
}
