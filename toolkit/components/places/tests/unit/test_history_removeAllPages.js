/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// Enable syncing for this test
start_sync();

// Get services.
let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bh = hs.QueryInterface(Ci.nsIBrowserHistory);
let mDBConn = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
let as = Cc["@mozilla.org/browser/annotation-service;1"].
         getService(Ci.nsIAnnotationService);
let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
let lms = Cc["@mozilla.org/browser/livemark-service;2"].
          getService(Ci.nsILivemarkService);

const kSyncFinished = "places-sync-finished";
// Number of expected sync notifications, we expect one per bookmark.
const EXPECTED_SYNCS = 4;

function add_fake_livemark() {
  let lmId = lms.createLivemarkFolderOnly(bs.bookmarksToolbarId, "Livemark",
                                          uri("http://www.mozilla.org/"),
                                          uri("http://www.mozilla.org/test.xml"),
                                          bs.DEFAULT_INDEX);
  // add a visited child
  bs.insertBookmark(lmId, uri("http://visited.livemark.com/"),
                    bs.DEFAULT_INDEX, "visited");
  hs.addVisit(uri("http://visited.livemark.com/"), Date.now(), null,
              hs.TRANSITION_BOOKMARK, false, 0);
  // add an unvisited child
  bs.insertBookmark(lmId, uri("http://unvisited.livemark.com/"),
                    bs.DEFAULT_INDEX, "unvisited");
}

let observer = {
  onBeginUpdateBatch: function() {
  },
  onEndUpdateBatch: function() {
  },
  onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {
  },
  onTitleChanged: function(aURI, aPageTitle) {
  },
  onDeleteURI: function(aURI) {
  },

  onClearHistory: function() {
    // check browserHistory returns no entries
    do_check_eq(0, bh.count);

    // Check that frecency for not cleared items (bookmarks) has been converted
    // to -MAX(visit_count, 1), so we will be able to recalculate frecency
    // starting from most frecent bookmarks. 
    // Memory table has been updated, disk table has not
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_places_temp WHERE frecency > 0 LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    stmt = mDBConn.createStatement(
      "SELECT h.id FROM moz_places_temp h WHERE h.frecency = -2 " +
        "AND EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) LIMIT 1");
    do_check_true(stmt.executeStep());
    stmt.finalize();

    // Check that all visit_counts have been brought to 0
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_places_temp WHERE visit_count <> 0 LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    // Check that history tables are empty
    stmt = mDBConn.createStatement(
      "SELECT * FROM (SELECT id FROM moz_historyvisits_temp LIMIT 1) " +
      "UNION ALL " +
      "SELECT * FROM (SELECT id FROM moz_historyvisits LIMIT 1)");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    // force a sync and check again disk tables, insertBookmark will do that
    bs.insertBookmark(bs.unfiledBookmarksFolder, uri("place:folder=4"),
                      bs.DEFAULT_INDEX, "shortcut");
  },

  onPageChanged: function(aURI, aWhat, aValue) {
  },
  onPageExpired: function(aURI, aVisitTime, aWholeEntry) {
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavHistoryObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}
hs.addObserver(observer, false);

let syncObserver = {
  _runCount: 0,
  observe: function (aSubject, aTopic, aData) {
    if (++this._runCount < EXPECTED_SYNCS)
      return;
    if (this._runCount == EXPECTED_SYNCS) {
      bh.removeAllPages();
      return;
    }

    // Sanity: check that places temp table is empty
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_places_temp LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    // Check that frecency for not cleared items (bookmarks) has been converted
    // to -MAX(visit_count, 1), so we will be able to recalculate frecency
    // starting from most frecent bookmarks.
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_places WHERE frecency > 0 LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    stmt = mDBConn.createStatement(
      "SELECT h.id FROM moz_places h WHERE h.frecency = -2 " +
        "AND EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) LIMIT 1");
    do_check_true(stmt.executeStep());
    stmt.finalize();

    // Check that all visit_counts have been brought to 0
    stmt = mDBConn.createStatement(
      "SELECT id FROM moz_places WHERE visit_count <> 0 LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();


    // Check that all moz_places entries except bookmarks and place: have been removed
    stmt = mDBConn.createStatement(
      "SELECT h.id FROM moz_places h WHERE SUBSTR(h.url,0,6) <> 'place:' "+
        "AND NOT EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    // Check that we only have favicons for retained places
    stmt = mDBConn.createStatement(
      "SELECT f.id FROM moz_favicons f WHERE NOT EXISTS " +
        "(SELECT id FROM moz_places WHERE favicon_id = f.id) LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    // Check that we only have annotations for retained places
    stmt = mDBConn.createStatement(
      "SELECT a.id FROM moz_annos a WHERE NOT EXISTS " +
        "(SELECT id FROM moz_places WHERE id = a.place_id) LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    // Check that we only have inputhistory for retained places
    stmt = mDBConn.createStatement(
      "SELECT i.place_id FROM moz_inputhistory i WHERE NOT EXISTS " +
        "(SELECT id FROM moz_places WHERE id = i.place_id) LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    // Check that place:uris have frecency 0
    stmt = mDBConn.createStatement(
      "SELECT h.id FROM moz_places h " +
      "WHERE SUBSTR(h.url,0,6) = 'place:' AND h.frecency <> 0 LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    // Check that livemarks children don't have frecency <> 0
    stmt = mDBConn.createStatement(
      "SELECT h.id FROM moz_places h " +
      "JOIN moz_bookmarks b ON h.id = b.fk " +
      "JOIN moz_bookmarks bp ON bp.id = b.parent " +
      "JOIN moz_items_annos t ON t.item_id = bp.id " +
      "JOIN moz_anno_attributes n ON t.anno_attribute_id = n.id " +
      "WHERE n.name = 'livemark/feedURI' AND h.frecency <> 0 LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    finish_test();
  }
}
os.addObserver(syncObserver, kSyncFinished, false);


// main
function run_test() {
  // Add a livemark with a visited and an unvisited child
  add_fake_livemark();

  // Add a bunch of visits
  hs.addVisit(uri("http://typed.mozilla.org"), Date.now(), null,
              hs.TRANSITION_TYPED, false, 0);

  hs.addVisit(uri("http://link.mozilla.org"), Date.now(), null,
              hs.TRANSITION_LINK, false, 0);
  hs.addVisit(uri("http://download.mozilla.org"), Date.now(), null,
              hs.TRANSITION_DOWNLOAD, false, 0);
  hs.addVisit(uri("http://invalid.mozilla.org"), Date.now(), null,
              0, false, 0); // invalid transition
  hs.addVisit(uri("http://redir_temp.mozilla.org"), Date.now(),
              uri("http://link.mozilla.org"), hs.TRANSITION_REDIRECT_TEMPORARY,
              true, 0);
  hs.addVisit(uri("http://redir_perm.mozilla.org"), Date.now(),
              uri("http://link.mozilla.org"), hs.TRANSITION_REDIRECT_PERMANENT,
              true, 0);

  // add a place: bookmark
  bs.insertBookmark(bs.unfiledBookmarksFolder, uri("place:folder=4"),
                    bs.DEFAULT_INDEX, "shortcut");
  
  // Add an expire never annotation
  // Actually expire never annotations are removed as soon as a page is removed
  // from the database, so this should act as a normal visit.
  as.setPageAnnotation(uri("http://download.mozilla.org"), "never", "never", 0,
                       as.EXPIRE_NEVER);

  // Add a bookmark
  // Bookmarked page should have history cleared and frecency = -old_visit_count
  // This will also finally sync temp tables to disk
  bs.insertBookmark(bs.unfiledBookmarksFolder, uri("http://typed.mozilla.org"),
                    bs.DEFAULT_INDEX, "bookmark");

  // this visit is not synced to disk
  hs.addVisit(uri("http://typed.mozilla.org"), Date.now(), null,
              hs.TRANSITION_BOOKMARK, false, 0);

  do_test_pending();
}
