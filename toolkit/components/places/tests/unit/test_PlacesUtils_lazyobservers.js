/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_test_pending();

  const TEST_URI = NetUtil.newURI("http://moz.org/")
  let observer = {
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsINavBookmarkObserver,
    ]),

    onBeginUpdateBatch: function () {},
    onEndUpdateBatch: function () {},
    onItemAdded: function (aItemId, aParentId, aIndex, aItemType, aURI) {
      do_check_true(aURI.equals(TEST_URI));
      PlacesUtils.removeLazyBookmarkObserver(this);
      do_test_finished();
    },
    onItemRemoved: function () {},
    onItemChanged: function () {},
    onItemVisited: function () {},
    onItemMoved: function () {},
  };

  // Check registration and removal with uninitialized bookmarks service.
  PlacesUtils.addLazyBookmarkObserver(observer);
  PlacesUtils.removeLazyBookmarkObserver(observer);

  // Add a proper lazy observer we will test.
  PlacesUtils.addLazyBookmarkObserver(observer);

  // Check that we don't leak when adding and removing an observer while the
  // bookmarks service is instantiated but no change happened (bug 721319).
  PlacesUtils.bookmarks;
  PlacesUtils.addLazyBookmarkObserver(observer);
  PlacesUtils.removeLazyBookmarkObserver(observer);
  try {
    PlacesUtils.bookmarks.removeObserver(observer);
    do_throw("Trying to remove a nonexisting observer should throw!");
  } catch (ex) {}

  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       TEST_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "Bookmark title");
}
