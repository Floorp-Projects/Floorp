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
 * Portions created by the Initial Developer are Copyright (C) 2009
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
 * This test ensures favicons are correctly expired by expireAllFavicons API,
 * both synced and unsynced favicons should be expired.
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
var icons = Cc["@mozilla.org/browser/favicon-service;1"].
            getService(Ci.nsIFaviconService);

const TEST_URI = "http://test.com/";
const TEST_ICON_URI = "http://test.com/favicon.ico";
const TEST_NOSYNC_URI = "http://nosync.test.com/";
const TEST_NOSYNC_ICON_URI = "http://nosync.test.com/favicon.ico";

const kSyncPrefName = "syncDBTableIntervalInSecs";
const SYNC_INTERVAL = 600;
const kSyncFinished = "places-sync-finished";

const kExpirationFinished = "places-favicons-expired";

var observer = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == kSyncFinished) {
      os.removeObserver(this, kSyncFinished);

      // Add a not synced page with a visit.
      hs.addVisit(uri(TEST_NOSYNC_URI), Date.now() * 1000, null,
                  hs.TRANSITION_TYPED, false, 0);
      // Set a favicon for the page.
      icons.setFaviconUrlForPage(uri(TEST_NOSYNC_URI), uri(TEST_NOSYNC_ICON_URI));

      // Check both a synced and a not-synced favicons exist.
      let stmt = dbConn.createStatement(
        "SELECT id FROM moz_places WHERE url = :url AND favicon_id NOT NULL");
      stmt.params["url"] = TEST_URI;
      do_check_true(stmt.executeStep());

      stmt.finalize();

      stmt = dbConn.createStatement(
        "SELECT id FROM moz_places_temp WHERE url = :url AND favicon_id NOT NULL");
      stmt.params["url"] = TEST_NOSYNC_URI;
      do_check_true(stmt.executeStep());

      stmt.finalize();

      // Expire all favicons.
      icons.expireAllFavicons();
    }
    else if (aTopic == kExpirationFinished) {
      os.removeObserver(this, kExpirationFinished);
      // Check all favicons have been removed.
      let stmt = dbConn.createStatement(
        "SELECT id FROM moz_favicons");
      do_check_false(stmt.executeStep());

      stmt.finalize();

      stmt = dbConn.createStatement(
        "SELECT id FROM moz_places_view WHERE favicon_id NOT NULL");
      do_check_false(stmt.executeStep());

      stmt.finalize();

      finish_test();
    }
  }
}
os.addObserver(observer, kSyncFinished, false);
os.addObserver(observer, kExpirationFinished, false);

function run_test()
{
  // First set the preference for the timer to a large value so we don't sync.
  prefs.setIntPref(kSyncPrefName, SYNC_INTERVAL);

  // Add a page with a visit.
  visitId = hs.addVisit(uri(TEST_URI), Date.now() * 1000, null,
                        hs.TRANSITION_TYPED, false, 0);
  // Set a favicon for the page.
  icons.setFaviconUrlForPage(uri(TEST_URI), uri(TEST_ICON_URI));

  // Now add a bookmark to force a sync.
  bs.insertBookmark(bs.toolbarFolder, uri(TEST_URI), bs.DEFAULT_INDEX, "visited");

  do_test_pending();
}
