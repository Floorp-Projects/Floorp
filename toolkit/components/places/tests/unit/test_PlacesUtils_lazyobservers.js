/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
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
    onBeforeItemRemoved: function () {},
    onItemRemoved: function () {},
    onItemChanged: function () {},
    onItemVisited: function () {},
    onItemMoved: function () {},
  };

  // Check registration and removal with uninitialized bookmarks service.
  PlacesUtils.addLazyBookmarkObserver(observer);
  PlacesUtils.removeLazyBookmarkObserver(observer);

  PlacesUtils.addLazyBookmarkObserver(observer);
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       TEST_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "Bookmark title");
}
