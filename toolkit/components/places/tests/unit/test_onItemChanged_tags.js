/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that changing a tag for a bookmark with multiple tags
// notifies OnItemChanged("tags") only once, and not once per tag.

function run_test() {
  do_test_pending();

  let tags = ["a", "b", "c"];
  let uri = NetUtil.newURI("http://1.moz.org/");

  let id = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.unfiledBookmarksFolderId, uri,
    PlacesUtils.bookmarks.DEFAULT_INDEX, "Bookmark 1"
  );
  PlacesUtils.tagging.tagURI(uri, tags);

  let bookmarksObserver = {
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsINavBookmarkObserver
    ]),

    _changedCount: 0,
    onItemChanged(aItemId, aProperty, aIsAnnotationProperty, aValue,
                            aLastModified, aItemType) {
      if (aProperty == "tags") {
        do_check_eq(aItemId, id);
        this._changedCount++;
      }
    },

    onItemRemoved(aItemId, aParentId, aIndex, aItemType) {
      if (aItemId == id) {
        PlacesUtils.bookmarks.removeObserver(this);
        do_check_eq(this._changedCount, 2);
        do_test_finished();
      }
    },

    onItemAdded() {},
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onItemVisited() {},
    onItemMoved() {},
  };
  PlacesUtils.bookmarks.addObserver(bookmarksObserver, false);

  PlacesUtils.tagging.tagURI(uri, ["d"]);
  PlacesUtils.tagging.tagURI(uri, ["e"]);
  PlacesUtils.bookmarks.removeItem(id);
}
