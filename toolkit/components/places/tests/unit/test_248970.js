/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Aaron Train.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Train <aaron.train@gmail.com> (Original Author)
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This unit test performs checks on the history testing area as outlined
// https://wiki.mozilla.org/Firefox3.1/PrivateBrowsing/TestPlan#History
// http://developer.mozilla.org/en/Using_the_Places_history_service

// Get global history service
try {
  var bhist = Cc["@mozilla.org/browser/global-history;2"].
                getService(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get global history service");
}

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service");
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service");
}

// Get the IO service
try {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Components.interfaces.nsIIOService);
} catch(ex) {
  do_throw("Could not get the io service");
}


/**
 * Function prohibits an attempt to pop up a confirmation
 * dialog box when entering Private Browsing mode.
 *
 * @returns a reference to the Private Browsing service
 */
var _PBSvc = null;
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
 * @returns the place id for aURI.
 */
function add_visit(aURI, aType) {
  var placeID = histsvc.addVisit(uri(aURI),
                                 Date.now() * 1000,
                                 null, // no referrer
                                 aType,
                                 false, // not redirect
                                 0);
  return placeID;
}

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @returns true if the URI is in the DB, false otherwise.
 */
function uri_in_db(aURI) {
  var options = histsvc.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  options.includeHidden = true;
  var query = histsvc.getNewQuery();
  query.uri = aURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  return (root.childCount == 1);
}

var visited_URIs = ["http://www.test-link.com/",
                    "http://www.test-typed.com/",
                    "http://www.test-bookmark.com/",
                    "http://www.test-redirect-permanent.com/",
                    "http://www.test-redirect-temporary.com/",
                    "http://www.test-embed.com",
                    "http://www.test-download.com"];

var nonvisited_URIs = ["http://www.google.ca/",
                       "http://www.google.com/",
                       "http://www.google.co.il/",
                       "http://www.google.fr/",
                       "http://www.google.es",
                       "http://www.google.com.tr",
                       "http://www.google.de"];
/**
 * Function fills history, one for each transition type.
 *
 * @returns nothing
 */
function fill_history_visitedURI() {
  add_visit(visited_URIs[0], histsvc.TRANSITION_LINK);
  add_visit(visited_URIs[1], histsvc.TRANSITION_TYPED);
  add_visit(visited_URIs[2], histsvc.TRANSITION_BOOKMARK);
  add_visit(visited_URIs[3], histsvc.TRANSITION_EMBED);
  add_visit(visited_URIs[4], histsvc.TRANSITION_REDIRECT_PERMANENT);
  add_visit(visited_URIs[5], histsvc.TRANSITION_REDIRECT_TEMPORARY);
  add_visit(visited_URIs[6], histsvc.TRANSITION_DOWNLOAD);
}

/**
 * Function fills history, one for each transition type.
 * second batch of history items
 *
 * @returns nothing
 */
function fill_history_nonvisitedURI() {
  add_visit(nonvisited_URIs[0], histsvc.TRANSITION_TYPED);
  add_visit(nonvisited_URIs[1], histsvc.TRANSITION_BOOKMARK);
  add_visit(nonvisited_URIs[2], histsvc.TRANSITION_LINK);
  add_visit(nonvisited_URIs[3], histsvc.TRANSITION_DOWNLOAD);
  add_visit(nonvisited_URIs[4], histsvc.TRANSITION_EMBED);
  add_visit(nonvisited_URIs[5], histsvc.TRANSITION_REDIRECT_PERMANENT);
  add_visit(nonvisited_URIs[6], histsvc.TRANSITION_REDIRECT_TEMPORARY);
}

// Initial batch of history items (7) + Bookmark_A (1)
// This number should not change after tests enter private browsing
// it will be set to 9 with the addition of Bookmark-B during private
// browsing mode.
var num_places_entries = 8; 

/**
 * Function performs a really simple query on our places entries,
 * and makes sure that the number of entries equal num_places_entries.
 *
 * @returns nothing
 */
function check_placesItem_Count(){
  // get bookmarks count
  var options = histsvc.getNewQueryOptions();
  options.includeHidden = true;
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  root.containerOpen = false;

  // get history item count
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
  query = histsvc.getNewQuery();
  result = histsvc.executeQuery(query, options);
  root = result.root;
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

var myBookmarks=new Array(2); // Bookmark-A
                              // Bookmark-B

function create_bookmark(aURI, aTitle, aKeyword) {
  var bookmarkID = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder,aURI,
                   bmsvc.DEFAULT_INDEX,aTitle);
  bmsvc.setKeywordForBookmark(bookmarkID,aKeyword);
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

  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 1; // should only expect Bookmark-A
  options.resultType = options.RESULT_TYPE_VISIT;

  var query  = histsvc.getNewQuery();
  query.setFolders([bmsvc.bookmarksMenuFolder],1);

  var result = histsvc.executeQuery(query, options);
  var root = result.root;

  root.containerOpen = true;

  do_check_eq(root.childCount,options.maxResults);

  var node = root.getChild(0);
  root.containerOpen = false;

  return (node.accessCount!=0);
}

function run_test() {

  // Fetch the private browsing service
  var pb = get_PBSvc();

  if(pb) { // Private Browsing might not be available
    start_sync(); // enable syncing

    // need to catch places sync notifications
    var os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    const kSyncFinished = "places-sync-finished";
    do_test_pending();

    var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
    prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);

    var bookmark_A_URI = ios.newURI("http://google.com/", null, null);
    var bookmark_B_URI = ios.newURI("http://bugzilla.mozilla.org/", null, null);
    var onBookmarkAAdded = {
      observe: function (aSubject, aTopic, aData) {
        os.removeObserver(this, kSyncFinished);

        check_placesItem_Count();

        // Bookmark-A should be bookmarked, data should be retrievable
        do_check_true(bmsvc.isBookmarked(bookmark_A_URI));
        do_check_eq("google",bmsvc.getKeywordForURI(bookmark_A_URI));

        // Enter Private Browsing Mode
        pb.privateBrowsingEnabled = true;

        // History items should not retrievable by isVisited
        for each(var visited_uri in visited_URIs)
          do_check_false(bhist.isVisited(uri(visited_uri)));

        // Check if Bookmark-A has been visited, should be false
        do_check_false(is_bookmark_A_altered());

        // Add a second set of history items during private browsing mode
        // should not be viewed/stored or in any way retrievable
        fill_history_nonvisitedURI();
        for each(var nonvisited_uri in nonvisited_URIs) {
          do_check_false(uri_in_db(uri(nonvisited_uri)));
          do_check_false(bhist.isVisited(uri(nonvisited_uri)));
        }

        // We attempted to add another 7 new entires, but we still have 7 history entries
        // and 1 history entry, Bookmark-A.
        // Private browsing blocked the entry of the new history entries
        check_placesItem_Count();

        // Check if Bookmark-A is still accessible
        do_check_true(bmsvc.isBookmarked(bookmark_A_URI));
        do_check_eq("google",bmsvc.getKeywordForURI(bookmark_A_URI));

        os.addObserver(onBookmarkBAdded, kSyncFinished, false);

        // Create Bookmark-B
        myBookmarks[1] = create_bookmark(bookmark_B_URI,"title 2", "bugzilla");
      }
    };
    var onBookmarkBAdded = {
      observe: function (aSubject, aTopic, aData) {
        os.removeObserver(this, kSyncFinished);

        // A check on the history count should be same as before, 7 history entries with
        // now 2 bookmark items (A) and bookmark (B), so we set num_places_entries to 9
        num_places_entries = 9; // Bookmark-B successfully added but not the history entries.
        check_placesItem_Count();

        // Exit Private Browsing Mode
        pb.privateBrowsingEnabled = false;

        // Check if Bookmark-B is still accessible
        do_check_true(bmsvc.isBookmarked(bookmark_B_URI));
        do_check_eq("bugzilla",bmsvc.getKeywordForURI(bookmark_B_URI));

        // Check if Bookmark-A is still accessible
        do_check_true(bmsvc.isBookmarked(bookmark_A_URI));
        do_check_eq("google",bmsvc.getKeywordForURI(bookmark_A_URI));

        // Check that the original set of history items are still accessible via isVisited
        for each(var visited_uri in visited_URIs) {
          do_check_true(uri_in_db(uri(visited_uri)));
          do_check_true(bhist.isVisited(uri(visited_uri)));
        }

        prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
        finish_test();
      }
    };

    // History database should be empty
    do_check_false(histsvc.hasHistoryEntries);

    // Create a handful of history items with various visit types
    fill_history_visitedURI();

    // History database should have entries
    do_check_true(histsvc.hasHistoryEntries);

    os.addObserver(onBookmarkAAdded, kSyncFinished, false);

    // Create Bookmark-A
    myBookmarks[0] = create_bookmark(bookmark_A_URI,"title 1", "google");

    // History items should be retrievable by query
    for each(var visited_uri in visited_URIs) {
      do_check_true(bhist.isVisited(uri(visited_uri)));
      do_check_true(uri_in_db(uri(visited_uri)));
    }
  }
}
