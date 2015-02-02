/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for bug 451499 <https://bugzilla.mozilla.org/show_bug.cgi?id=451499>:
 * Wrong folder icon appears on smart bookmarks.
 */

////////////////////////////////////////////////////////////////////////////////
/// Globals

const PAGE_URI = NetUtil.newURI("http://example.com/test_query_result");

////////////////////////////////////////////////////////////////////////////////
/// Tests

function run_test()
{
  run_next_test();
}

add_test(function test_query_result_favicon_changed_on_child()
{
  // Bookmark our test page, so it will appear in the query resultset.
  let testBookmark = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.bookmarksMenuFolderId,
    PAGE_URI,
    PlacesUtils.bookmarks.DEFAULT_INDEX,
    "test_bookmark");

  // Get the last 10 bookmarks added to the menu or the toolbar.
  let query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarksMenuFolderId,
                    PlacesUtils.toolbarFolderId], 2);

  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 10;
  options.excludeQueries = 1;
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;

  let result = PlacesUtils.history.executeQuery(query, options);
  let resultObserver = {
    __proto__: NavHistoryResultObserver.prototype,
    containerStateChanged: function QRFCOC_containerStateChanged(aContainerNode,
                                                                 aOldState,
                                                                 aNewState) {
      if (aNewState == Ci.nsINavHistoryContainerResultNode.STATE_OPENED) {
        // We set a favicon on PAGE_URI while the container is open.  The
        // favicon for the page must have data associated with it in order for
        // the icon changed notifications to be sent, so we use a valid image
        // data URI.
        PlacesUtils.favicons.setAndFetchFaviconForPage(PAGE_URI,
                                                       SMALLPNG_DATA_URI,
                                                       false,
                                                       PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);
      }
    },
    nodeIconChanged: function QRFCOC_nodeIconChanged(aNode) {
      do_throw("The icon should be set only for the page," +
               " not for the containing query.");
    }
  };
  result.addObserver(resultObserver, false);

  waitForFaviconChanged(PAGE_URI, SMALLPNG_DATA_URI,
                        function QRFCOC_faviconChanged() {
    // We must wait for the asynchronous database thread to finish the
    // operation, and then for the main thread to process any pending
    // notifications that came from the asynchronous thread, before we can be
    // sure that nodeIconChanged was not invoked in the meantime.
    PlacesTestUtils.promiseAsyncUpdates().then(function QRFCOC_asyncUpdates() {
      do_execute_soon(function QRFCOC_soon() {
        result.removeObserver(resultObserver);

        // Free the resources immediately.
        result.root.containerOpen = false;
        run_next_test();
      });
    });
  });

  // Open the container and wait for containerStateChanged.
  result.root.containerOpen = true;
});
