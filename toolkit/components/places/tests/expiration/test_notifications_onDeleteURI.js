/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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
 * The Original Code is Places Unit Tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net> (Original Author)
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
 * What this is aimed to test:
 *
 * Expiring a full page should fire an onDeleteURI notification.
 */

let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

let gTests = [

  { desc: "Add 1 bookmarked page.",
    addPages: 1,
    addBookmarks: 1,
    expectedNotifications: 0, // No expirable pages.
  },

  { desc: "Add 2 pages, 1 bookmarked.",
    addPages: 2,
    addBookmarks: 1,
    expectedNotifications: 1, // Only one expirable page.
  },

  { desc: "Add 10 pages, none bookmarked.",
    addPages: 10,
    addBookmarks: 0,
    expectedNotifications: 10, // Will expire everything.
  },

  { desc: "Add 10 pages, all bookmarked.",
    addPages: 10,
    addBookmarks: 10,
    expectedNotifications: 0, // No expirable pages.
  },

];

let gCurrentTest;
let gTestIndex = 0;

function run_test() {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Expire anything that is expirable.
  setMaxPages(0);

  do_test_pending();
  run_next_test();
}

function run_next_test() {
  if (gTests.length) {
    gCurrentTest = gTests.shift();
    gTestIndex++;
    print("\nTEST " + gTestIndex + ": " + gCurrentTest.desc);
    gCurrentTest.receivedNotifications = 0;

    // Setup visits.
    let now = getExpirablePRTime();
    for (let i = 0; i < gCurrentTest.addPages; i++) {
      let page = "http://" + gTestIndex + "." + i + ".mozilla.org/";
      hs.addVisit(uri(page), now++, null, hs.TRANSITION_TYPED, false, 0);
    }

    // Setup bookmarks.
    gCurrentTest.bookmarks = [];
    for (let i = 0; i < gCurrentTest.addBookmarks; i++) {
      let page = "http://" + gTestIndex + "." + i + ".mozilla.org/";
      bs.insertBookmark(bs.unfiledBookmarksFolder, uri(page),
                        bs.DEFAULT_INDEX, null);
      gCurrentTest.bookmarks.push(page);
    }

    // Observe history.
    historyObserver = {
      onBeginUpdateBatch: function PEX_onBeginUpdateBatch() {},
      onEndUpdateBatch: function PEX_onEndUpdateBatch() {},
      onClearHistory: function() {},
      onVisit: function() {},
      onTitleChanged: function() {},
      onBeforeDeleteURI: function() {},
      onDeleteURI: function(aURI, aGUID, aReason) {
        gCurrentTest.receivedNotifications++;
        // Check this uri was not bookmarked.
        do_check_eq(gCurrentTest.bookmarks.indexOf(aURI.spec), -1);
        do_check_valid_places_guid(aGUID);
        do_check_eq(aReason, Ci.nsINavHistoryObserver.REASON_EXPIRED);
      },
      onPageChanged: function() {},
      onDeleteVisits: function(aURI, aTime) { },
    };
    hs.addObserver(historyObserver, false);

    // Observe expirations.
    observer = {
      observe: function(aSubject, aTopic, aData) {
        os.removeObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED);
        hs.removeObserver(historyObserver, false);

        // This test finished.
        check_result();
      }
    };
    os.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

    // Expire now, observers will check results.
    force_expiration_step(-1);
  }
  else {
    clearMaxPages();
    bs.removeFolderChildren(bs.unfiledBookmarksFolder);
    waitForClearHistory(do_test_finished);
  }
}

function check_result() {

  do_check_eq(gCurrentTest.receivedNotifications,
              gCurrentTest.expectedNotifications);

  // Clean up.
  bs.removeFolderChildren(bs.unfiledBookmarksFolder);
  waitForClearHistory(run_next_test);
}
