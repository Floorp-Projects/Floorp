/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * Expiring only visits for a page, but not the full page, should fire an
 * onDeleteVisits notification.
 */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);

var tests = [

  { desc: "Add 1 bookmarked page.",
    addPages: 1,
    visitsPerPage: 1,
    addBookmarks: 1,
    limitExpiration: -1,
    expectedNotifications: 1, // Will expire visits for 1 page.
  },

  { desc: "Add 2 pages, 1 bookmarked.",
    addPages: 2,
    visitsPerPage: 1,
    addBookmarks: 1,
    limitExpiration: -1,
    expectedNotifications: 1, // Will expire visits for 1 page.
  },

  { desc: "Add 10 pages, none bookmarked.",
    addPages: 10,
    visitsPerPage: 1,
    addBookmarks: 0,
    limitExpiration: -1,
    expectedNotifications: 0, // Will expire only full pages.
  },

  { desc: "Add 10 pages, all bookmarked.",
    addPages: 10,
    visitsPerPage: 1,
    addBookmarks: 10,
    limitExpiration: -1,
    expectedNotifications: 10, // Will expire visist for all pages.
  },

  { desc: "Add 10 pages with lot of visits, none bookmarked.",
    addPages: 10,
    visitsPerPage: 10,
    addBookmarks: 0,
    limitExpiration: 10,
    expectedNotifications: 10, // Will expire 1 visist for each page, but won't
  },                           // expire pages since they still have visits.

];

add_task(async function test_notifications_onDeleteVisits() {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Expire anything that is expirable.
  setMaxPages(0);

  for (let testIndex = 1; testIndex <= tests.length; testIndex++) {
    let currentTest = tests[testIndex - 1];
    print("\nTEST " + testIndex + ": " + currentTest.desc);
    currentTest.receivedNotifications = 0;

    // Setup visits.
    let timeInMicroseconds = getExpirablePRTime(8);

    function newTimeInMicroseconds() {
      timeInMicroseconds = timeInMicroseconds + 1000;
      return timeInMicroseconds;
    }

    for (let j = 0; j < currentTest.visitsPerPage; j++) {
      for (let i = 0; i < currentTest.addPages; i++) {
        let page = "http://" + testIndex + "." + i + ".mozilla.org/";
        await PlacesTestUtils.addVisits({ uri: uri(page), visitDate: newTimeInMicroseconds() });
      }
    }

    // Setup bookmarks.
    currentTest.bookmarks = [];
    for (let i = 0; i < currentTest.addBookmarks; i++) {
      let page = "http://" + testIndex + "." + i + ".mozilla.org/";
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        title: null,
        url: page
      });
      currentTest.bookmarks.push(page);
    }

    // Observe history.
    let historyObserver = {
      onBeginUpdateBatch: function PEX_onBeginUpdateBatch() {},
      onEndUpdateBatch: function PEX_onEndUpdateBatch() {},
      onClearHistory() {},
      onTitleChanged() {},
      onDeleteURI(aURI, aGUID, aReason) {
        // Check this uri was not bookmarked.
        Assert.equal(currentTest.bookmarks.indexOf(aURI.spec), -1);
        do_check_valid_places_guid(aGUID);
        Assert.equal(aReason, Ci.nsINavHistoryObserver.REASON_EXPIRED);
      },
      onPageChanged() {},
      onDeleteVisits(aURI, aTime, aGUID, aReason) {
        currentTest.receivedNotifications++;
        do_check_guid_for_uri(aURI, aGUID);
        Assert.equal(aReason, Ci.nsINavHistoryObserver.REASON_EXPIRED);
      },
    };
    hs.addObserver(historyObserver);

    // Expire now.
    await promiseForceExpirationStep(currentTest.limitExpiration);

    hs.removeObserver(historyObserver, false);

    Assert.equal(currentTest.receivedNotifications,
                 currentTest.expectedNotifications);

    // Clean up.
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  }

  clearMaxPages();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});
