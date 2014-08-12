/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test performs checks on the history testing area as outlined
// https://wiki.mozilla.org/Firefox3.1/PrivateBrowsing/TestPlan#History
// http://developer.mozilla.org/en/Using_the_Places_history_service

let visitedURIs = [
  "http://www.test-link.com/",
  "http://www.test-typed.com/",
  "http://www.test-bookmark.com/",
  "http://www.test-redirect-permanent.com/",
  "http://www.test-redirect-temporary.com/",
  "http://www.test-embed.com/",
  "http://www.test-framed.com/",
  "http://www.test-download.com/"
].map(NetUtil.newURI.bind(NetUtil));

add_task(function () {
  let windowsToClose = [];
  let placeItemsCount = 0;

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  yield promiseClearHistory();

  // History database should be empty
  is(PlacesUtils.history.hasHistoryEntries, false,
     "History database should be empty");

   // Ensure we wait for the default bookmarks import.
  let bookmarksDeferred = Promise.defer();
  waitForCondition(() => {
    placeItemsCount = getPlacesItemsCount();
    return placeItemsCount > 0
  }, bookmarksDeferred.resolve, "Should have default bookmarks");
  yield bookmarksDeferred.promise;

  // Create a handful of history items with various visit types
  yield promiseAddVisits([
    { uri: visitedURIs[0], transition: TRANSITION_LINK },
    { uri: visitedURIs[1], transition: TRANSITION_TYPED },
    { uri: visitedURIs[2], transition: TRANSITION_BOOKMARK },
    { uri: visitedURIs[3], transition: TRANSITION_REDIRECT_PERMANENT },
    { uri: visitedURIs[4], transition: TRANSITION_REDIRECT_TEMPORARY },
    { uri: visitedURIs[5], transition: TRANSITION_EMBED },
    { uri: visitedURIs[6], transition: TRANSITION_FRAMED_LINK },
    { uri: visitedURIs[7], transition: TRANSITION_DOWNLOAD }
  ]);

  // History database should have entries
  is(PlacesUtils.history.hasHistoryEntries, true,
     "History database should have entries");

  placeItemsCount += 7;
  // We added 7 new items to history.
  is(getPlacesItemsCount(), placeItemsCount,
     "Check the total items count");

  function* testOnWindow(aIsPrivate, aCount) {
    let deferred = Promise.defer();
    whenNewWindowLoaded({ private: aIsPrivate }, deferred.resolve);
    let win = yield deferred.promise;
    windowsToClose.push(win);

    // History items should be retrievable by query
    yield checkHistoryItems();

    // Updates the place items count
    let count = getPlacesItemsCount();

    // Create Bookmark
    let bookmarkTitle = "title " + windowsToClose.length;
    let bookmarkKeyword = "keyword " + windowsToClose.length;
    let bookmarkUri = NetUtil.newURI("http://test-a-" + windowsToClose.length + ".com/");

    let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                                                  bookmarkUri,
                                                  PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                  bookmarkTitle);
    PlacesUtils.bookmarks.setKeywordForBookmark(id, bookmarkKeyword);
    count++;

    ok(PlacesUtils.bookmarks.isBookmarked(bookmarkUri),
       "Bookmark should be bookmarked, data should be retrievable");
    is(bookmarkKeyword, PlacesUtils.bookmarks.getKeywordForURI(bookmarkUri),
       "Check bookmark uri keyword");
    is(getPlacesItemsCount(), count,
       "Check the new bookmark items count");
    is(isBookmarkAltered(), false, "Check if bookmark has been visited");
  }

  // Test on windows.
  yield testOnWindow(false);
  yield testOnWindow(true);
  yield testOnWindow(false);
});

/**
 * Function performs a really simple query on our places entries,
 * and makes sure that the number of entries equal num_places_entries.
 */
function getPlacesItemsCount() {
  // Get bookmarks count
  let options = PlacesUtils.history.getNewQueryOptions();
  options.includeHidden = true;
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  let root = PlacesUtils.history.executeQuery(
    PlacesUtils.history.getNewQuery(), options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  root.containerOpen = false;

  // Get history item count
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
  let root = PlacesUtils.history.executeQuery(
    PlacesUtils.history.getNewQuery(), options).root;
  root.containerOpen = true;
  cc += root.childCount;
  root.containerOpen = false;

  return cc;
}

function* checkHistoryItems() {
  for (let i = 0; i < visitedURIs.length; i++) {
    let visitedUri = visitedURIs[i];
    ok((yield promiseIsURIVisited(visitedUri)), "");
    if (/embed/.test(visitedUri.spec)) {
      is(!!pageInDatabase(visitedUri), false, "Check if URI is in database");
    } else {
      ok(!!pageInDatabase(visitedUri), "Check if URI is in database");
    }
  }
}

/**
 * Checks if an address is found in the database.
 * @param aURI
 *        nsIURI or address to look for.
 * @return place id of the page or 0 if not found
 */
function pageInDatabase(aURI) {
  let url = (aURI instanceof Ci.nsIURI ? aURI.spec : aURI);
  let stmt = DBConn().createStatement(
    "SELECT id FROM moz_places WHERE url = :url"
  );
  stmt.params.url = url;
  try {
    if (!stmt.executeStep())
      return 0;
    return stmt.getInt64(0);
  } finally {
    stmt.finalize();
  }
}

/**
 * Function attempts to check if Bookmark-A has been visited
 * during private browsing mode, function should return false
 *
 * @returns false if the accessCount has not changed
 *          true if the accessCount has changed
 */
function isBookmarkAltered(){
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 1; // should only expect a new bookmark

  let query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.bookmarksMenuFolder], 1);

  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  is(root.childCount, options.maxResults, "Check new bookmarks results");
  let node = root.getChild(0);
  root.containerOpen = false;

  return (node.accessCount != 0);
}
