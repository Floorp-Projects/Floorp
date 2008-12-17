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

 /*
  * This test checks that embed visits are not synced down to disk, we hold
  * them in memory since they're going to be purged at session close
  */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var dbConn = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch("places.");
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

const TEST_URI = "http://test.com/";
const EMBED_URI = "http://embed.test.com/";
const PLACE_URI = "place:test.com/";

const kSyncPrefName = "syncDBTableIntervalInSecs";
const SYNC_INTERVAL = 1;
const kSyncFinished = "places-sync-finished";

var transitions = [ hs.TRANSITION_LINK,
                    hs.TRANSITION_TYPED,
                    hs.TRANSITION_BOOKMARK,
                    hs.TRANSITION_EMBED,
                    hs.TRANSITION_REDIRECT_PERMANENT,
                    hs.TRANSITION_REDIRECT_TEMPORARY,
                    hs.TRANSITION_DOWNLOAD ];

var observer = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == kSyncFinished && this.visitId != -1) {
      // remove the observer, we don't need to observe sync on quit
      os.removeObserver(this, kSyncFinished);
      dump("\n\n");
      dump_table("moz_places_temp");
      dump_table("moz_places");
      dump("\n\n");
      // Check that moz_places table has been correctly synced
      var stmt = dbConn.createStatement(
        "SELECT id FROM moz_places WHERE url = :url");
      stmt.params["url"] = TEST_URI;
      do_check_true(stmt.executeStep());
      stmt.finalize();
      stmt = dbConn.createStatement(
        "SELECT id FROM moz_places_temp WHERE url = :url");
      stmt.params["url"] = TEST_URI;
      do_check_false(stmt.executeStep());
      stmt.finalize();

      stmt = dbConn.createStatement(
        "SELECT id FROM moz_places WHERE url = :url");
      stmt.params["url"] = EMBED_URI;
      do_check_false(stmt.executeStep());
      stmt.finalize();
      stmt = dbConn.createStatement(
        "SELECT id FROM moz_places_temp WHERE url = :url");
      stmt.params["url"] = EMBED_URI;
      do_check_true(stmt.executeStep());
      stmt.finalize();

      stmt = dbConn.createStatement(
        "SELECT id FROM moz_places WHERE url = :url");
      stmt.params["url"] = PLACE_URI;
      do_check_true(stmt.executeStep());
      stmt.finalize();
      stmt = dbConn.createStatement(
        "SELECT id FROM moz_places_temp WHERE url = :url");
      stmt.params["url"] = PLACE_URI;
      do_check_false(stmt.executeStep());
      stmt.finalize();

      // Check that all visits but embed ones are in disk table
      stmt = dbConn.createStatement(
        "SELECT count(*) FROM moz_historyvisits h WHERE visit_type <> :t_embed");
      stmt.params["t_embed"] = hs.TRANSITION_EMBED;
      do_check_true(stmt.executeStep());
      do_check_eq(stmt.getInt32(0), (transitions.length - 1) * 2);
      stmt.finalize();
      stmt = dbConn.createStatement(
        "SELECT id FROM moz_historyvisits h WHERE visit_type = :t_embed");
      stmt.params["t_embed"] = hs.TRANSITION_EMBED;
      do_check_false(stmt.executeStep());
      stmt.finalize();
      stmt = dbConn.createStatement(
        "SELECT id FROM moz_historyvisits_temp h WHERE visit_type = :t_embed");
      stmt.params["t_embed"] = hs.TRANSITION_EMBED;
      do_check_true(stmt.executeStep());
      stmt.finalize();
      stmt = dbConn.createStatement(
        "SELECT id FROM moz_historyvisits_temp h WHERE visit_type <> :t_embed");
      stmt.params["t_embed"] = hs.TRANSITION_EMBED;
      do_check_false(stmt.executeStep());
      stmt.finalize();

      do_test_finished();
    }
  }
}

function run_test()
{
  // First set the preference for the timer to a small value
  prefs.setIntPref(kSyncPrefName, SYNC_INTERVAL);

  // Add a visit for every transition type on TEST_URI
  // Embed visit should stay in temp table, while other should be synced
  // Place entry for this uri should be synced to disk table
  transitions.forEach(function addVisit(aTransition) {
                        hs.addVisit(uri(TEST_URI), Date.now() * 1000, null,
                                    aTransition, false, 0);
                      });

  // Add an embed visit for EMBED_URI
  // Embed visit should stay in temp table
  // Place entry for this uri should stay in temp table
  hs.addVisit(uri(EMBED_URI), Date.now() * 1000, null,
              hs.TRANSITION_EMBED, false, 0);

  // Add a visit for every transition type on PLACE_URI
  // Embed visit should stay in temp table
  // Place entry for this uri should be synced to disk table
  transitions.forEach(function addVisit(aTransition) {
                        hs.addVisit(uri(PLACE_URI), Date.now() * 1000, null,
                                    aTransition, false, 0);
                      });

  os.addObserver(observer, kSyncFinished, false);

  do_test_pending();
}
