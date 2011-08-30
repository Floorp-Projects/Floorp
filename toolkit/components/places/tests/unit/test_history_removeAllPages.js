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

let mDBConn = DBConn();

function add_fake_livemark() {
  let lmId = PlacesUtils.livemarks.createLivemarkFolderOnly(
    PlacesUtils.toolbarFolderId, "Livemark",
    uri("http://www.mozilla.org/"), uri("http://www.mozilla.org/test.xml"),
    PlacesUtils.bookmarks.DEFAULT_INDEX
  );
  // Add a visited child.
  PlacesUtils.bookmarks.insertBookmark(lmId,
                                       uri("http://visited.livemark.com/"),
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "visited");
  PlacesUtils.history.addVisit(uri("http://visited.livemark.com/"),
                               Date.now(), null,
                               Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
                               false, 0);
  // Add an unvisited child.
  PlacesUtils.bookmarks.insertBookmark(lmId,
                                       uri("http://unvisited.livemark.com/"),
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "unvisited");
}

let historyObserver = {
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {},
  onVisit: function() {},
  onTitleChanged: function() {},
  onBeforeDeleteURI: function() {},
  onDeleteURI: function(aURI) {},
  onPageChanged: function() {},
  onDeleteVisits: function() {},

  onClearHistory: function() {
    PlacesUtils.history.removeObserver(this, false);

    // check browserHistory returns no entries
    do_check_eq(0, PlacesUtils.bhistory.count);

    Services.obs.addObserver(function observeExpiration(aSubject, aTopic, aData)
    {
      Services.obs.removeObserver(observeExpiration, aTopic, false);

      waitForAsyncUpdates(function () {
        // Check that frecency for not cleared items (bookmarks) has been converted
        // to -MAX(visit_count, 1), so we will be able to recalculate frecency
        // starting from most frecent bookmarks.
        stmt = mDBConn.createStatement(
          "SELECT h.id FROM moz_places h WHERE h.frecency > 0 ");
        do_check_false(stmt.executeStep());
        stmt.finalize();

        stmt = mDBConn.createStatement(
          "SELECT h.id FROM moz_places h WHERE h.frecency < 0 " +
            "AND EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) LIMIT 1");
        do_check_true(stmt.executeStep());
        stmt.finalize();

        // Check that all visit_counts have been brought to 0
        stmt = mDBConn.createStatement(
          "SELECT id FROM moz_places WHERE visit_count <> 0 LIMIT 1");
        do_check_false(stmt.executeStep());
        stmt.finalize();

        // Check that history tables are empty
        stmt = mDBConn.createStatement(
          "SELECT * FROM (SELECT id FROM moz_historyvisits LIMIT 1)");
        do_check_false(stmt.executeStep());
        stmt.finalize();

        // Check that all moz_places entries except bookmarks and place: have been removed
        stmt = mDBConn.createStatement(
          "SELECT h.id FROM moz_places h WHERE SUBSTR(h.url, 1, 6) <> 'place:' "+
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
          "WHERE SUBSTR(h.url, 1, 6) = 'place:' AND h.frecency <> 0 LIMIT 1");
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

        do_test_finished();
      });
    }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavHistoryObserver,
  ]),
}
PlacesUtils.history.addObserver(historyObserver, false);

function run_test() {
  // places-init-complete is notified after run_test, and it will
  // run a first frecency fix through async statements.
  // To avoid random failures we have to run after all of this.
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    Services.obs.removeObserver(arguments.callee, aTopic, false);
    do_execute_soon(continue_test);
  }, PlacesUtils.TOPIC_INIT_COMPLETE, false);

  do_test_pending();
}

function continue_test() {
  // Add a livemark with a visited and an unvisited child
  add_fake_livemark();
  PlacesUtils.history.addVisit(uri("http://typed.mozilla.org/"), Date.now(),
                               null, Ci.nsINavHistoryService.TRANSITION_TYPED,
                               false, 0);
  PlacesUtils.history.addVisit(uri("http://link.mozilla.org/"), Date.now(),
                               null, Ci.nsINavHistoryService.TRANSITION_LINK,
                               false, 0);
  PlacesUtils.history.addVisit(uri("http://download.mozilla.org/"), Date.now(),
                               null, Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
                               false, 0);
  PlacesUtils.history.addVisit(uri("http://invalid.mozilla.org/"), Date.now(),
                               null, 0, false, 0); // Use an invalid transition.
  PlacesUtils.history.addVisit(uri("http://redir_temp.mozilla.org/"), Date.now(),
                               uri("http://link.mozilla.org/"),
                               Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY,
                               true, 0);
  PlacesUtils.history.addVisit(uri("http://redir_perm.mozilla.org/"), Date.now(),
                               uri("http://link.mozilla.org/"),
                               Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
                               true, 0);

  // add a place: bookmark
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       uri("place:folder=4"),
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "shortcut");
  
  // Add an expire never annotation
  // Actually expire never annotations are removed as soon as a page is removed
  // from the database, so this should act as a normal visit.
  PlacesUtils.annotations.setPageAnnotation(uri("http://download.mozilla.org/"),
                                            "never", "never", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);

  // Add a bookmark
  // Bookmarked page should have history cleared and frecency = -old_visit_count
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       uri("http://typed.mozilla.org/"),
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark");

  PlacesUtils.history.addVisit(uri("http://typed.mozilla.org/"), Date.now(),
                               null, PlacesUtils.history.TRANSITION_BOOKMARK,
                               false, 0);

  // Add a last page and wait for its frecency.
  PlacesUtils.history.addVisit(uri("http://frecency.mozilla.org/"), Date.now(),
                               null, Ci.nsINavHistoryService.TRANSITION_LINK,
                               false, 0);
  waitForFrecency("http://frecency.mozilla.org/", function (aFrecency) aFrecency > 0,
                  function () {
                    PlacesUtils.bhistory.removeAllPages();
                  }, this, []);
}
