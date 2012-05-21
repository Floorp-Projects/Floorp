/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
