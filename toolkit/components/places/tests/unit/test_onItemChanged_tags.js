/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that changing a tag for a bookmark with multiple tags
// notifies OnItemChanged("tags") only once, and not once per tag.

add_task(async function run_test() {
  let tags = ["a", "b", "c"];
  let uri = Services.io.newURI("http://1.moz.org/");

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: uri,
    title: "Bookmark 1"
  });
  PlacesUtils.tagging.tagURI(uri, tags);

  let promise = PromiseUtils.defer();

  let bookmarksObserver = {
    QueryInterface: ChromeUtils.generateQI([
      Ci.nsINavBookmarkObserver
    ]),

    _changedCount: 0,
    onItemChanged(aItemId, aProperty, aIsAnnotationProperty, aValue,
                            aLastModified, aItemType, aParentId, aGuid) {
      if (aProperty == "tags") {
        Assert.equal(aGuid, bookmark.guid);
        this._changedCount++;
      }
    },

    onItemRemoved(aItemId, aParentId, aIndex, aItemType, aURI, aGuid) {
      if (aGuid == bookmark.guid) {
        PlacesUtils.bookmarks.removeObserver(this);
        Assert.equal(this._changedCount, 2);
        promise.resolve();
      }
    },

    onItemAdded() {},
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onItemVisited() {},
    onItemMoved() {},
  };
  PlacesUtils.bookmarks.addObserver(bookmarksObserver);

  PlacesUtils.tagging.tagURI(uri, ["d"]);
  PlacesUtils.tagging.tagURI(uri, ["e"]);

  await promise;

  await PlacesUtils.bookmarks.remove(bookmark.guid);
});
