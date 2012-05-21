/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This unit test performs checks on the history testing area as outlined
// https://wiki.mozilla.org/Firefox3.1/PrivateBrowsing/TestPlan#History
// http://developer.mozilla.org/en/Using_the_Places_history_service

/**
 * Function prohibits an attempt to pop up a confirmation
 * dialog box when entering Private Browsing mode.
 *
 * @return a reference to the Private Browsing service
 */
let _PBSvc = null;
function get_PBSvc() {
  if (_PBSvc)
    return _PBSvc;

  try {
    _PBSvc = Components.classes["@mozilla.org/privatebrowsing;1"].
             getService(Components.interfaces.nsIPrivateBrowsingService);
    return _PBSvc;
  } catch (e) {}
  return null;
}

/**
 * Adds a test URI visit to the database
 *
 * @param aURI
 *        The URI to add a visit for.
 * @param aType
 *        Transition type for the URI.
 */
function add_visit(aURI, aType) {
  PlacesUtils.history.addVisit(uri(aURI), Date.now() * 1000, null, aType,
                               false, 0);
}

let visited_URIs = ["http://www.test-link.com/",
                    "http://www.test-typed.com/",
                    "http://www.test-bookmark.com/",
                    "http://www.test-redirect-permanent.com/",
                    "http://www.test-redirect-temporary.com/",
                    "http://www.test-embed.com/",
                    "http://www.test-framed.com/",
                    "http://www.test-download.com/"];

let nonvisited_URIs = ["http://www.google.ca/typed/",
                       "http://www.google.com/bookmark/",
                       "http://www.google.co.il/link/",
                       "http://www.google.fr/download/",
                       "http://www.google.es/embed/",
                       "http://www.google.it/framed-link/",
                       "http://www.google.com.tr/redirect-permanent/",
                       "http://www.google.de/redirect-temporary/"];
/**
 * Function fills history, one for each transition type.
 */
function fill_history_visitedURI() {
  PlacesUtils.history.runInBatchMode({
    runBatched: function (aUserData) {
      add_visit(visited_URIs[0], PlacesUtils.history.TRANSITION_LINK);
      add_visit(visited_URIs[1], PlacesUtils.history.TRANSITION_TYPED);
      add_visit(visited_URIs[2], PlacesUtils.history.TRANSITION_BOOKMARK);
      add_visit(visited_URIs[3], PlacesUtils.history.TRANSITION_REDIRECT_PERMANENT);
      add_visit(visited_URIs[4], PlacesUtils.history.TRANSITION_REDIRECT_TEMPORARY);
      add_visit(visited_URIs[5], PlacesUtils.history.TRANSITION_EMBED);
      add_visit(visited_URIs[6], PlacesUtils.history.TRANSITION_FRAMED_LINK);
      add_visit(visited_URIs[7], PlacesUtils.history.TRANSITION_DOWNLOAD);
    }
  }, null);
}

/**
 * Function fills history, one for each transition type.
 * second batch of history items
 */
function fill_history_nonvisitedURI() {
  PlacesUtils.history.runInBatchMode({
    runBatched: function (aUserData) {
      add_visit(nonvisited_URIs[0], PlacesUtils.history.TRANSITION_TYPED);
      add_visit(nonvisited_URIs[1], PlacesUtils.history.TRANSITION_BOOKMARK);
      add_visit(nonvisited_URIs[2], PlacesUtils.history.TRANSITION_LINK);
      add_visit(nonvisited_URIs[3], PlacesUtils.history.TRANSITION_DOWNLOAD);
      add_visit(nonvisited_URIs[4], PlacesUtils.history.TRANSITION_EMBED);
      add_visit(nonvisited_URIs[5], PlacesUtils.history.TRANSITION_FRAMED_LINK);
      add_visit(nonvisited_URIs[6], PlacesUtils.history.TRANSITION_REDIRECT_PERMANENT);
      add_visit(nonvisited_URIs[7], PlacesUtils.history.TRANSITION_REDIRECT_TEMPORARY);
    }
  }, null);
}

// Initial batch of history items (7) + Bookmark_A (1)
// This number should not change after tests enter private browsing
// it will be set to 10 with the addition of Bookmark-B during private
// browsing mode.
let num_places_entries = 8;

/**
 * Function performs a really simple query on our places entries,
 * and makes sure that the number of entries equal num_places_entries.
 */
function check_placesItem_Count(){
  // get bookmarks count
  let options = PlacesUtils.history.getNewQueryOptions();
  options.includeHidden = true;
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  root.containerOpen = false;

  // get history item count
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  cc += root.childCount;
  root.containerOpen = false;

  // check the total count
  do_check_eq(cc,num_places_entries);
}

/**
 * Function creates a bookmark
 * @param aURI
 *        The URI for the bookmark
 * @param aTitle
 *        The title for the bookmark
 * @param aKeyword
 *        The keyword for the bookmark
 * @returns the bookmark
 */

let myBookmarks = new Array(2); // Bookmark-A and Bookmark-B

function create_bookmark(aURI, aTitle, aKeyword) {
  let bookmarkID = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                                                        aURI,
                                                        PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                        aTitle);
  PlacesUtils.bookmarks.setKeywordForBookmark(bookmarkID, aKeyword);
  return bookmarkID;
}

/**
 * Function attempts to check if Bookmark-A has been visited
 * during private browsing mode, function should return false
 *
 * @returns false if the accessCount has not changed
 *          true if the accessCount has changed
 */
function is_bookmark_A_altered(){

  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 1; // should only expect Bookmark-A
  options.resultType = options.RESULT_TYPE_VISIT;

  let query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.bookmarksMenuFolder],1);

  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  do_check_eq(root.childCount,options.maxResults);
  let node = root.getChild(0);
  root.containerOpen = false;

  return (node.accessCount!=0);
}

function run_test() {
  // Private Browsing might not be available.
  let pb = get_PBSvc();
  if (!pb) {
    return;
  }

  do_test_pending();

  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  let bookmark_A_URI = NetUtil.newURI("http://google.com/");
  let bookmark_B_URI = NetUtil.newURI("http://bugzilla.mozilla.org/");

  let onBookmarkAAdded = function() {
    check_placesItem_Count();

    // Bookmark-A should be bookmarked, data should be retrievable
    do_check_true(PlacesUtils.bookmarks.isBookmarked(bookmark_A_URI));
    do_check_eq("google", PlacesUtils.bookmarks.getKeywordForURI(bookmark_A_URI));

    // Enter Private Browsing Mode
    pb.privateBrowsingEnabled = true;

    // History items should not retrievable by isVisited
    visited_URIs.forEach(function (visited_uri) {
      do_check_false(PlacesUtils.bhistory.isVisited(uri(visited_uri)));
    });

    // Check if Bookmark-A has been visited, should be false
    do_check_false(is_bookmark_A_altered());

    // Add a second set of history items during private browsing mode
    // should not be viewed/stored or in any way retrievable
    fill_history_nonvisitedURI();
    nonvisited_URIs.forEach(function (nonvisited_uri) {
      do_check_false(!!page_in_database(nonvisited_uri));
      do_check_false(PlacesUtils.bhistory.isVisited(uri(nonvisited_uri)));
    });

    // We attempted to add another 7 new entries, but we still have 7 history entries
    // and 1 history entry, Bookmark-A.
    // Private browsing blocked the entry of the new history entries
    check_placesItem_Count();

    // Check if Bookmark-A is still accessible
    do_check_true(PlacesUtils.bookmarks.isBookmarked(bookmark_A_URI));
    do_check_eq("google",PlacesUtils.bookmarks.getKeywordForURI(bookmark_A_URI));

    // Create Bookmark-B
    myBookmarks[1] = create_bookmark(bookmark_B_URI,"title 2", "bugzilla");
    onBookmarkBAdded();
  };

  let onBookmarkBAdded = function() {
    // A check on the history count should be same as before, 7 history entries with
    // now 2 bookmark items (A) and bookmark (B), so we increase num_places_entries.
    num_places_entries++; // Bookmark-B successfully added but not the history entries.
    check_placesItem_Count();

    // Exit Private Browsing Mode
    pb.privateBrowsingEnabled = false;

    // Check if Bookmark-B is still accessible
    do_check_true(PlacesUtils.bookmarks.isBookmarked(bookmark_B_URI));
    do_check_eq("bugzilla",PlacesUtils.bookmarks.getKeywordForURI(bookmark_B_URI));

    // Check if Bookmark-A is still accessible
    do_check_true(PlacesUtils.bookmarks.isBookmarked(bookmark_A_URI));
    do_check_eq("google",PlacesUtils.bookmarks.getKeywordForURI(bookmark_A_URI));

    // Check that the original set of history items are still accessible via isVisited
    visited_URIs.forEach(function (visited_uri) {
      do_check_true(PlacesUtils.bhistory.isVisited(uri(visited_uri)));
      if (/embed/.test(visited_uri)) {
        do_check_false(!!page_in_database(visited_uri));
      }
      else {
        do_check_true(!!page_in_database(visited_uri));
      }
    });

    Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
    do_test_finished();
  };

  // History database should be empty
  do_check_false(PlacesUtils.history.hasHistoryEntries);

  // Create a handful of history items with various visit types
  fill_history_visitedURI();

  // History database should have entries
  do_check_true(PlacesUtils.history.hasHistoryEntries);

  // Create Bookmark-A
  myBookmarks[0] = create_bookmark(bookmark_A_URI,"title 1", "google");

  // History items should be retrievable by query
  visited_URIs.forEach(function (visited_uri) {
    do_check_true(PlacesUtils.bhistory.isVisited(uri(visited_uri)));
    if (/embed/.test(visited_uri)) {
      do_check_false(!!page_in_database(visited_uri));
    }
    else {
      do_check_true(!!page_in_database(visited_uri));
    }
  });

  onBookmarkAAdded();
}
