/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);

let gTests = [

  { desc: "Set max_pages to a negative value, with 1 page.",
    maxPages: -1,
    addPages: 1,
    expectedNotifications: 0, // Will ignore and won't expire anything.
  },

  { desc: "Set max_pages to 0.",
    maxPages: 0,
    addPages: 1,
    expectedNotifications: 1,
  },

  { desc: "Set max_pages to 0, with 2 pages.",
    maxPages: 0,
    addPages: 2,
    expectedNotifications: 2, // Will expire everything.
  },

  // Notice if we are over limit we do a full step of expiration.  So we ensure
  // that we will expire if we are over the limit, but we don't ensure that we
  // will expire exactly up to the limit.  Thus in this case we expire
  // everything.
  { desc: "Set max_pages to 1 with 2 pages.",
    maxPages: 1,
    addPages: 2,
    expectedNotifications: 2, // Will expire everything (in this case).
  },

  { desc: "Set max_pages to 10, with 9 pages.",
    maxPages: 10,
    addPages: 9,
    expectedNotifications: 0, // We are at the limit, won't expire anything.
  },

  { desc: "Set max_pages to 10 with 10 pages.",
    maxPages: 10,
    addPages: 10,
    expectedNotifications: 0, // We are below the limit, won't expire anything.
  },
];

let gCurrentTest;
let gTestIndex = 0;

function run_test() {
  // The pref should not exist by default.
  try {
    getMaxPages();
    do_throw("interval pref should not exist by default");
  }
  catch (ex) {}

  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

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
      hs.addVisit(uri("http://" + gTestIndex + "." + i + ".mozilla.org/"), now++, null,
                  hs.TRANSITION_TYPED, false, 0);
    }

    // Observe history.
    historyObserver = {
      onBeginUpdateBatch: function PEX_onBeginUpdateBatch() {},
      onEndUpdateBatch: function PEX_onEndUpdateBatch() {},
      onClearHistory: function() {},
      onVisit: function() {},
      onTitleChanged: function() {},
      onBeforeDeleteURI: function() {},
      onDeleteURI: function(aURI) {
        print("onDeleteURI " + aURI.spec);
        gCurrentTest.receivedNotifications++;
      },
      onPageChanged: function() {},
      onDeleteVisits: function(aURI, aTime) {
        print("onDeleteVisits " + aURI.spec + " " + aTime);
      },
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

    setMaxPages(gCurrentTest.maxPages);
    // Expire now, observers will check results.
    force_expiration_step(-1);
  }
  else {
    clearMaxPages();
    waitForClearHistory(do_test_finished);
  }
}

function check_result() {

  do_check_eq(gCurrentTest.receivedNotifications,
              gCurrentTest.expectedNotifications);

  // Clean up.
  waitForClearHistory(run_next_test);
}
