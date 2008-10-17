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
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *  Marco Bonardo <mak77@bonardo.net>
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

/**
 * This test ensures that when adding a visit, then syncing, and adding another
 * visit to the same url creates two visits and that we only end up with one
 * entry in moz_places.
 */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var db = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsPIPlacesDatabase).
         DBConnection;
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch("places.");

const TEST_URI = "http://test.com/";

const kSyncPrefName = "syncDBTableIntervalInSecs";
const SYNC_INTERVAL = 1;

function run_test()
{
  // First set the preference for the timer to a small value
  prefs.setIntPref(kSyncPrefName, SYNC_INTERVAL);

  // Now add the first visit
  let id = hs.addVisit(uri(TEST_URI), Date.now() * 1000, null,
                       hs.TRANSITION_TYPED, false, 0);

  // Check the visit, but after enough time has passed for the DB flush service
  // to have fired it's timer.
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback({
    notify: function(aTimer)
    {
      new_test_visit_uri_event(id, TEST_URI, true);

      // Get the place_id and pass it on
      let db = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
      let stmt = db.createStatement(
        "SELECT place_id " +
        "FROM moz_historyvisits " +
        "WHERE id = ?"
      );
      stmt.bindInt64Parameter(0, id);
      do_check_true(stmt.executeStep());
      continue_test(id, stmt.getInt64(0));
      stmt.finalize();
      stmt = null;
    }
  }, (SYNC_INTERVAL * 1000) * 2, Ci.nsITimer.TYPE_ONE_SHOT);
  do_test_pending();
}

function continue_test(aLastVisitId, aPlaceId)
{
  // Now we add another visit
  let id = hs.addVisit(uri(TEST_URI), Date.now() * 1000, null,
                       hs.TRANSITION_TYPED, false, 0);
  do_check_neq(aLastVisitId, id);

  // Check the visit, but after enough time has passed for the DB flush service
  // to have fired it's timer.
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback({
    notify: function(aTimer)
    {
      new_test_visit_uri_event(id, TEST_URI, true);

      // Check to make sure we have the same place_id
      let db = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
      let stmt = db.createStatement(
        "SELECT * " +
        "FROM moz_historyvisits " +
        "WHERE id = ?1 " +
        "AND place_id = ?2"
      );
      stmt.bindInt64Parameter(0, id);
      stmt.bindInt64Parameter(1, aPlaceId);
      do_check_true(stmt.executeStep());
      stmt.finalize();
      stmt = null;

      finish_test();
    }
  }, (SYNC_INTERVAL * 1000) * 2, Ci.nsITimer.TYPE_ONE_SHOT);
}
