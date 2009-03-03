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
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bh = Cc["@mozilla.org/browser/global-history;2"].
         getService(Ci.nsIBrowserHistory);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

const TEST_URI = "http://test.com/";

const kSyncPrefName = "places.syncDBTableIntervalInSecs";
const SYNC_INTERVAL = 1;
const kSyncFinished = "places-sync-finished";

const kExpireDaysPrefName = "browser.history_expire_days";
const EXPIRE_DAYS = 1;

var observer = {
  visitId: -1,
  notificationReceived: false,
  _syncCount: 0,
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == kSyncFinished) {
      // We won't have expired on the first sync (we expire first, then sync),
      // so we have to wait for the second sync to check.
      if (this._syncCount++ == 0)
        return;

      // Sanity check to make sure that we actually added a visit.
      do_check_neq(this.visitId, -1);

      // Ensure that we received a notification about expiration.
      do_check_false(this.notificationReceived);

      // remove the observer, we don't need to observe sync on quit
      os.removeObserver(this, kSyncFinished);
      hs.removeObserver(historyObserver, false);

      // Check the visit
      new_test_visit_uri_event(this.visitId, TEST_URI, true, true);
    }
  }
}
os.addObserver(observer, kSyncFinished, false);

// Used to ensure that we do not expire anything.
var historyObserver = {
  onPageExpired: function(aURI, aVisitTime, aWholeEntry)
  {
    do_throw("How did we get called?!");
    observer.notificationReceived = true;
  }
}
hs.addObserver(historyObserver, false);

function run_test()
{
  // First set our preferences to small values.
  prefs.setIntPref(kExpireDaysPrefName, EXPIRE_DAYS);
  prefs.setIntPref(kSyncPrefName, SYNC_INTERVAL);

  // Sanity check to ensure that our test uri is not already visited.
  do_check_false(bh.isVisited(uri(TEST_URI)));

  // Now add the visit two days ago.
  observer.visitId = hs.addVisit(uri(TEST_URI), Date.now() * 1000, null,
                                 hs.TRANSITION_TYPED, false, 0);

  do_test_pending();
}
