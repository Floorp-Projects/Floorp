/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_lazyBookmarksObservers() {
  const TEST_URI = Services.io.newURI("http://moz.org/");

  let promise = PromiseUtils.defer();

  let observer = {
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsINavBookmarkObserver,
    ]),

    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI) {
      do_check_true(aURI.equals(TEST_URI));
      PlacesUtils.removeLazyBookmarkObserver(this);
      promise.resolve();
    },
    onItemRemoved() {},
    onItemChanged() {},
    onItemVisited() {},
    onItemMoved() {},
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

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URI,
    title: "Bookmark title"
  });

  await promise;
});
