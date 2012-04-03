/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test checks that bookmarks service is correctly forwarding async
 * events like visit or favicon additions. */

const NOW = Date.now() * 1000;

let observer = {
  bookmarks: [],
  observedBookmarks: 0,
  visitId: 0,
  reset: function ()
  {
    this.observedBookmarks = 0;
  },
  onBeginUpdateBatch: function () {},
  onEndUpdateBatch: function () {},
  onItemAdded: function () {},
  onBeforeItemRemoved: function () {},
  onItemRemoved: function () {},
  onItemMoved: function () {},
  onItemChanged: function(aItemId, aProperty, aIsAnnotation, aNewValue,
                          aLastModified, aItemType)
  {
    do_log_info("Check that we got the correct change information.");
    do_check_neq(this.bookmarks.indexOf(aItemId), -1);
    if (aProperty == "favicon") {
      do_check_false(aIsAnnotation);
      do_check_eq(aNewValue, SMALLPNG_DATA_URI.spec);
      do_check_eq(aLastModified, 0);
      do_check_eq(aItemType, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    }
    else if (aProperty == "cleartime") {
      do_check_false(aIsAnnotation);
      do_check_eq(aNewValue, "");
      do_check_eq(aLastModified, 0);
      do_check_eq(aItemType, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    }
    else {
      do_throw("Unexpected property change " + aProperty);
    }

    if (++this.observedBookmarks == this.bookmarks.length) {
      run_next_test();
    }
  },
  onItemVisited: function(aItemId, aVisitId, aTime)
  {
    do_log_info("Check that we got the correct visit information.");
    do_check_neq(this.bookmarks.indexOf(aItemId), -1);
    do_check_eq(aVisitId, this.visitId);
    do_check_eq(aTime, NOW);
    if (++this.observedBookmarks == this.bookmarks.length) {
      run_next_test();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
  ])
};
PlacesUtils.bookmarks.addObserver(observer, false);

let gTests = [
  function add_visit_test()
  {
    observer.reset();
    // Add a visit to the bookmark and wait for the observer.
    observer.visitId =
      PlacesUtils.history.addVisit(NetUtil.newURI("http://book.ma.rk/"), NOW, null,
                                   PlacesUtils.history.TRANSITION_TYPED, false, 0);
  },
  function add_icon_test()
  {
    observer.reset();
    PlacesUtils.favicons.setAndFetchFaviconForPage(NetUtil.newURI("http://book.ma.rk/"),
                                                   SMALLPNG_DATA_URI, true);
  },
  function remove_page_test()
  {
    observer.reset();
    PlacesUtils.history.removePage(NetUtil.newURI("http://book.ma.rk/"));
  },
  function cleanup()
  {
    PlacesUtils.bookmarks.removeObserver(observer, false);
    run_next_test();
  },
];

function run_test()
{
  // Add multiple bookmarks to the same uri.
  observer.bookmarks.push(
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         NetUtil.newURI("http://book.ma.rk/"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "Bookmark")
  );
  observer.bookmarks.push(
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                         NetUtil.newURI("http://book.ma.rk/"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "Bookmark")
  );

  run_next_test();
}
