/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * Expiring a full page should fire an onDeleteURI notification.
 */

let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

let tests = [

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

function run_test() {
  run_next_test();
}

add_task(function test_notifications_onDeleteURI() {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Expire anything that is expirable.
  setMaxPages(0);

  for (let testIndex = 1; testIndex <= tests.length; testIndex++) {
    let currentTest = tests[testIndex -1];
    print("\nTEST " + testIndex + ": " + currentTest.desc);
    currentTest.receivedNotifications = 0;

    // Setup visits.
    let now = getExpirablePRTime();
    for (let i = 0; i < currentTest.addPages; i++) {
      let page = "http://" + testIndex + "." + i + ".mozilla.org/";
      yield PlacesTestUtils.addVisits({ uri: uri(page), visitDate: now++ });
    }

    // Setup bookmarks.
    currentTest.bookmarks = [];
    for (let i = 0; i < currentTest.addBookmarks; i++) {
      let page = "http://" + testIndex + "." + i + ".mozilla.org/";
      bs.insertBookmark(bs.unfiledBookmarksFolder, uri(page),
                        bs.DEFAULT_INDEX, null);
      currentTest.bookmarks.push(page);
    }

    // Observe history.
    historyObserver = {
      onBeginUpdateBatch: function PEX_onBeginUpdateBatch() {},
      onEndUpdateBatch: function PEX_onEndUpdateBatch() {},
      onClearHistory: function() {},
      onVisit: function() {},
      onTitleChanged: function() {},
      onDeleteURI: function(aURI, aGUID, aReason) {
        currentTest.receivedNotifications++;
        // Check this uri was not bookmarked.
        do_check_eq(currentTest.bookmarks.indexOf(aURI.spec), -1);
        do_check_valid_places_guid(aGUID);
        do_check_eq(aReason, Ci.nsINavHistoryObserver.REASON_EXPIRED);
      },
      onPageChanged: function() {},
      onDeleteVisits: function(aURI, aTime) { },
    };
    hs.addObserver(historyObserver, false);

    // Expire now.
    yield promiseForceExpirationStep(-1);

    hs.removeObserver(historyObserver, false);

    do_check_eq(currentTest.receivedNotifications,
                currentTest.expectedNotifications);

    // Clean up.
    bs.removeFolderChildren(bs.unfiledBookmarksFolder);
    yield PlacesTestUtils.clearHistory();
  }

  clearMaxPages();
  bs.removeFolderChildren(bs.unfiledBookmarksFolder);
  yield PlacesTestUtils.clearHistory();
});
