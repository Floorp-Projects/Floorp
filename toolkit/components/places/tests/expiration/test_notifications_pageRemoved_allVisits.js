/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * Expiring only visits for a page, but not the full page, should fire an
 * page-removed for all visits notification.
 */

var tests = [
  {
    desc: "Add 1 bookmarked page.",
    addPages: 1,
    visitsPerPage: 1,
    addBookmarks: 1,
    limitExpiration: -1,
    expectedNotifications: 1, // Will expire visits for 1 page.
    expectedIsPartialRemoval: true,
  },

  {
    desc: "Add 2 pages, 1 bookmarked.",
    addPages: 2,
    visitsPerPage: 1,
    addBookmarks: 1,
    limitExpiration: -1,
    expectedNotifications: 1, // Will expire visits for 1 page.
    expectedIsPartialRemoval: true,
  },

  {
    desc: "Add 10 pages, none bookmarked.",
    addPages: 10,
    visitsPerPage: 1,
    addBookmarks: 0,
    limitExpiration: -1,
    expectedNotifications: 0, // Will expire only full pages.
    expectedIsPartialRemoval: false,
  },

  {
    desc: "Add 10 pages, all bookmarked.",
    addPages: 10,
    visitsPerPage: 1,
    addBookmarks: 10,
    limitExpiration: -1,
    expectedNotifications: 10, // Will expire visits for all pages.
    expectedIsPartialRemoval: true,
  },

  {
    desc: "Add 10 pages with lot of visits, none bookmarked.",
    addPages: 10,
    visitsPerPage: 10,
    addBookmarks: 0,
    limitExpiration: 10,
    expectedNotifications: 10, // Will expire 1 visit for each page, but won't
    // expire pages since they still have visits.
    expectedIsPartialRemoval: true,
  },
];

add_task(async () => {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Expire anything that is expirable.
  setMaxPages(0);

  for (let testIndex = 1; testIndex <= tests.length; testIndex++) {
    let currentTest = tests[testIndex - 1];
    info("TEST " + testIndex + ": " + currentTest.desc);
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
        await PlacesTestUtils.addVisits({
          uri: uri(page),
          visitDate: newTimeInMicroseconds(),
        });
      }
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
    let notificationsHandled = new Promise(resolve => {
      const listener = async events => {
        for (const event of events) {
          Assert.equal(event.type, "page-removed");
          Assert.equal(event.reason, PlacesVisitRemoved.REASON_EXPIRED);

          if (event.isRemovedFromStore) {
            // Check this uri was not bookmarked.
            Assert.equal(currentTest.bookmarks.indexOf(event.url), -1);
            do_check_valid_places_guid(event.pageGuid);
          } else {
            currentTest.receivedNotifications++;
            await check_guid_for_uri(
              Services.io.newURI(event.url),
              event.pageGuid
            );
            Assert.equal(
              event.isPartialVisistsRemoval,
              currentTest.expectedIsPartialRemoval,
              "Should have the correct flag setting for partial removal"
            );
          }
        }
        PlacesObservers.removeListener(["page-removed"], listener);
        resolve();
      };
      PlacesObservers.addListener(["page-removed"], listener);
    });

    // Expire now.
    await promiseForceExpirationStep(currentTest.limitExpiration);
    await notificationsHandled;

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
