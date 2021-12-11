/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * Expiring a full page should fire an page-removed event notification.
 */

var tests = [
  {
    desc: "Add 1 bookmarked page.",
    addPages: 1,
    addBookmarks: 1,
    expectedNotifications: 0, // No expirable pages.
  },

  {
    desc: "Add 2 pages, 1 bookmarked.",
    addPages: 2,
    addBookmarks: 1,
    expectedNotifications: 1, // Only one expirable page.
  },

  {
    desc: "Add 10 pages, none bookmarked.",
    addPages: 10,
    addBookmarks: 0,
    expectedNotifications: 10, // Will expire everything.
  },

  {
    desc: "Add 10 pages, all bookmarked.",
    addPages: 10,
    addBookmarks: 10,
    expectedNotifications: 0, // No expirable pages.
  },
];

add_task(async () => {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Expire anything that is expirable.
  setMaxPages(0);

  for (let testIndex = 1; testIndex <= tests.length; testIndex++) {
    let currentTest = tests[testIndex - 1];
    print("\nTEST " + testIndex + ": " + currentTest.desc);
    currentTest.receivedNotifications = 0;

    // Setup visits.
    let now = getExpirablePRTime();
    for (let i = 0; i < currentTest.addPages; i++) {
      let page = "http://" + testIndex + "." + i + ".mozilla.org/";
      await PlacesTestUtils.addVisits({ uri: uri(page), visitDate: now++ });
    }

    // Setup bookmarks.
    currentTest.bookmarks = [];
    for (let i = 0; i < currentTest.addBookmarks; i++) {
      let page = "http://" + testIndex + "." + i + ".mozilla.org/";
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        title: null,
        url: page,
      });
      currentTest.bookmarks.push(page);
    }

    // Observe history.
    const listener = events => {
      for (const event of events) {
        Assert.equal(event.type, "page-removed");

        if (!event.isRemovedFromStore) {
          continue;
        }

        currentTest.receivedNotifications++;
        // Check this uri was not bookmarked.
        Assert.equal(currentTest.bookmarks.indexOf(event.url), -1);
        do_check_valid_places_guid(event.pageGuid);
        Assert.equal(event.reason, PlacesVisitRemoved.REASON_EXPIRED);
      }
    };
    PlacesObservers.addListener(["page-removed"], listener);

    // Expire now.
    await promiseForceExpirationStep(-1);

    PlacesObservers.removeListener(["page-removed"], listener);

    Assert.equal(
      currentTest.receivedNotifications,
      currentTest.expectedNotifications
    );

    // Clean up.
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  }

  clearMaxPages();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});
