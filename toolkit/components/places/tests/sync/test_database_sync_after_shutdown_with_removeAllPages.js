/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
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

var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch("places.");
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bh = hs.QueryInterface(Ci.nsIBrowserHistory);
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

const TEST_URI = "http://test.com/";

const PREF_SYNC_INTERVAL = "syncDBTableIntervalInSecs";
const SYNC_INTERVAL = 600; // ten minutes
const TOPIC_SYNC_FINISHED = "places-sync-finished";
const TOPIC_CONNECTION_CLOSED = "places-connection-closed";

var historyObserver = {
  visitId: -1,
  cleared: false,
  onVisit: function(aURI, aVisitId, aTime, aSessionId, aReferringId,
                    aTransitionType, aAdded) {
    this.visitId = aVisitId;
  },
  onClearHistory: function() {
    // check browserHistory returns no entries
    do_check_eq(0, bh.count);
    this.cleared = true;
    hs.removeObserver(this);
  }
}
hs.addObserver(historyObserver, false);

var observer = {
  _runCount: 0,
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == TOPIC_SYNC_FINISHED) {
      if (++this._runCount == 1) {
        // The first sync is due to the insert bookmark.
        // Simulate a clear private data just before shutdown.
        bh.removeAllPages();
        os.addObserver(shutdownObserver, TOPIC_CONNECTION_CLOSED, false);
        // Immediately notify shutdown.
        shutdownPlaces();
        return;
      }

      // Remove the observer, we don't need it anymore.
      os.removeObserver(this, TOPIC_SYNC_FINISHED);

      // Visit id must be valid.
      do_check_neq(historyObserver.visitId, -1);
      // History must have been cleared.
      do_check_true(historyObserver.cleared);
    }
  }
}
os.addObserver(observer, TOPIC_SYNC_FINISHED, false);

let shutdownObserver = {
  observe: function(aSubject, aTopic, aData) {
    os.removeObserver(this, aTopic);
    do_check_false(PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection.connectionReady);

    let dbConn = DBConn();
    do_check_true(dbConn.connectionReady);

    // Check that frecency for not cleared items (bookmarks) has been
    // converted to -MAX(visit_count, 1), so we will be able to
    // recalculate frecency starting from most frecent bookmarks.
    let stmt = dbConn.createStatement(
      "SELECT id FROM moz_places WHERE frecency > 0 LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    stmt = dbConn.createStatement(
      "SELECT h.id FROM moz_places h WHERE h.frecency = -2 " +
        "AND EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) LIMIT 1");
    do_check_true(stmt.executeStep());
    stmt.finalize();

    // Check that all visit_counts have been brought to 0
    stmt = dbConn.createStatement(
      "SELECT id FROM moz_places WHERE visit_count <> 0 LIMIT 1");
    do_check_false(stmt.executeStep());
    stmt.finalize();

    dbConn.asyncClose(function() {
      do_check_false(dbConn.connectionReady);

      do_test_finished();
    });
  }
}

function run_test()
{
  do_test_pending();

  // Set the preference for the timer to a really large value, so it won't
  // run before the test finishes.
  prefs.setIntPref(PREF_SYNC_INTERVAL, SYNC_INTERVAL);

  // Now add a visit before creating bookmark, and one later
  hs.addVisit(uri(TEST_URI), Date.now() * 1000, null,
              hs.TRANSITION_TYPED, false, 0);
  bs.insertBookmark(bs.unfiledBookmarksFolder, uri(TEST_URI),
                    bs.DEFAULT_INDEX, "bookmark");
  hs.addVisit(uri(TEST_URI), Date.now() * 1000, null,
              hs.TRANSITION_TYPED, false, 0);
}
