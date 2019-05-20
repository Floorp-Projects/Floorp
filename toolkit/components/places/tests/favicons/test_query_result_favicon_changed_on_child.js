/**
 * Test for bug 451499 <https://bugzilla.mozilla.org/show_bug.cgi?id=451499>:
 * Wrong folder icon appears on queries.
 */

"use strict";

add_task(async function test_query_result_favicon_changed_on_child() {
  // Bookmark our test page, so it will appear in the query resultset.
  const PAGE_URI = Services.io.newURI("http://example.com/test_query_result");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "test_bookmark",
    url: PAGE_URI,
  });

  // Get the last 10 bookmarks added to the menu or the toolbar.
  let query = PlacesUtils.history.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.menuGuid,
                    PlacesUtils.bookmarks.toolbarGuid]);

  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 10;
  options.excludeQueries = 1;
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;

  let result = PlacesUtils.history.executeQuery(query, options);
  let resultObserver = {
    __proto__: NavHistoryResultObserver.prototype,
    containerStateChanged(aContainerNode, aOldState, aNewState) {
      if (aNewState == Ci.nsINavHistoryContainerResultNode.STATE_OPENED) {
        // We set a favicon on PAGE_URI while the container is open.  The
        // favicon for the page must have data associated with it in order for
        // the icon changed notifications to be sent, so we use a valid image
        // data URI.
        PlacesUtils.favicons.setAndFetchFaviconForPage(PAGE_URI,
                                                       SMALLPNG_DATA_URI,
                                                       false,
                                                       PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                                       null,
                                                       Services.scriptSecurityManager.getSystemPrincipal());
      }
    },
    nodeIconChanged(aNode) {
      do_throw("The icon should be set only for the page," +
               " not for the containing query.");
    },
  };
  result.addObserver(resultObserver);

  // Open the container and wait for containerStateChanged. We should start
  // observing before setting |containerOpen| as that's caused by the
  // setAndFetchFaviconForPage() call caused by the containerStateChanged
  // observer above.
  let promise = promiseFaviconChanged(PAGE_URI, SMALLPNG_DATA_URI);
  result.root.containerOpen = true;
  await promise;

  // We must wait for the asynchronous database thread to finish the
  // operation, and then for the main thread to process any pending
  // notifications that came from the asynchronous thread, before we can be
  // sure that nodeIconChanged was not invoked in the meantime.
  await PlacesTestUtils.promiseAsyncUpdates();
  result.removeObserver(resultObserver);

  // Free the resources immediately.
  result.root.containerOpen = false;

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function test_query_result_favicon_changed_not_affect_lastmodified() {
  // Bookmark our test page, so it will appear in the query resultset.
  const PAGE_URI2 = Services.io.newURI("http://example.com/test_query_result");
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "test_bookmark",
    url: PAGE_URI2,
  });

  let result = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.menuGuid);

  Assert.equal(result.root.childCount, 1,
    "Should have only one item in the query");
  Assert.equal(result.root.getChild(0).uri, PAGE_URI2.spec,
    "Should have the correct child");
  Assert.equal(result.root.getChild(0).lastModified, PlacesUtils.toPRTime(bm.lastModified),
    "Should have the expected last modified date.");

  let promise = promiseFaviconChanged(PAGE_URI2, SMALLPNG_DATA_URI);
  PlacesUtils.favicons.setAndFetchFaviconForPage(PAGE_URI2,
                                                 SMALLPNG_DATA_URI,
                                                 false,
                                                 PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                                 null,
                                                 Services.scriptSecurityManager.getSystemPrincipal());
  await promise;

  // Open the container and wait for containerStateChanged. We should start
  // observing before setting |containerOpen| as that's caused by the
  // setAndFetchFaviconForPage() call caused by the containerStateChanged
  // observer above.

  // We must wait for the asynchronous database thread to finish the
  // operation, and then for the main thread to process any pending
  // notifications that came from the asynchronous thread, before we can be
  // sure that nodeIconChanged was not invoked in the meantime.
  await PlacesTestUtils.promiseAsyncUpdates();

  Assert.equal(result.root.childCount, 1,
    "Should have only one item in the query");
  Assert.equal(result.root.getChild(0).uri, PAGE_URI2.spec,
    "Should have the correct child");
  Assert.equal(result.root.getChild(0).lastModified, PlacesUtils.toPRTime(bm.lastModified),
    "Should not have changed the last modified date.");

  // Free the resources immediately.
  result.root.containerOpen = false;
});
