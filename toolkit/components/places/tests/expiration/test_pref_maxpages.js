/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * Expiration will obey to hardware spec, but user can set a custom maximum
 * number of pages to retain, to restrict history, through
 * "places.history.expiration.max_pages".
 * This limit is used at next expiration run.
 * If the pref is set to a number < 0 we will use the default value.
 */

var tests = [
  {
    desc: "Set max_pages to a negative value, with 1 page.",
    maxPages: -1,
    addPages: 1,
    expectedNotifications: 0, // Will ignore and won't expire anything.
  },

  {
    desc: "Set max_pages to 0.",
    maxPages: 0,
    addPages: 1,
    expectedNotifications: 1,
  },

  {
    desc: "Set max_pages to 0, with 2 pages.",
    maxPages: 0,
    addPages: 2,
    expectedNotifications: 2, // Will expire everything.
  },

  // Notice if we are over limit we do a full step of expiration.  So we ensure
  // that we will expire if we are over the limit, but we don't ensure that we
  // will expire exactly up to the limit.  Thus in this case we expire
  // everything.
  {
    desc: "Set max_pages to 1 with 2 pages.",
    maxPages: 1,
    addPages: 2,
    expectedNotifications: 2, // Will expire everything (in this case).
  },

  {
    desc: "Set max_pages to 10, with 9 pages.",
    maxPages: 10,
    addPages: 9,
    expectedNotifications: 0, // We are at the limit, won't expire anything.
  },

  {
    desc: "Set max_pages to 10 with 10 pages.",
    maxPages: 10,
    addPages: 10,
    expectedNotifications: 0, // We are below the limit, won't expire anything.
  },
];

add_task(async function test_pref_maxpages() {
  // The pref should not exist by default.
  try {
    getMaxPages();
    do_throw("interval pref should not exist by default");
  } catch (ex) {}

  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  for (let testIndex = 1; testIndex <= tests.length; testIndex++) {
    let currentTest = tests[testIndex - 1];
    print("\nTEST " + testIndex + ": " + currentTest.desc);
    currentTest.receivedNotifications = 0;

    // Setup visits.
    let now = getExpirablePRTime();
    for (let i = 0; i < currentTest.addPages; i++) {
      let page = "http://" + testIndex + "." + i + ".mozilla.org/";
      await PlacesTestUtils.addVisits({ uri: uri(page), visitDate: now-- });
    }

    const listener = events => {
      for (const event of events) {
        print("page-removed " + event.url);
        Assert.equal(event.type, "page-removed");
        Assert.ok(event.isRemovedFromStore);
        Assert.equal(event.reason, PlacesVisitRemoved.REASON_EXPIRED);
        currentTest.receivedNotifications++;
      }
    };
    PlacesObservers.addListener(["page-removed"], listener);

    setMaxPages(currentTest.maxPages);

    // Expire now.
    await promiseForceExpirationStep(-1);

    PlacesObservers.removeListener(["page-removed"], listener);

    Assert.equal(
      currentTest.receivedNotifications,
      currentTest.expectedNotifications
    );

    // Clean up.
    await PlacesUtils.history.clear();
  }

  clearMaxPages();
  await PlacesUtils.history.clear();
});
