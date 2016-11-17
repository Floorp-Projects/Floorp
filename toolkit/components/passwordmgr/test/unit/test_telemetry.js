/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the statistics and other counters reported through telemetry.
 */

"use strict";

// Globals

const MS_PER_DAY = 24 * 60 * 60 * 1000;

// To prevent intermittent failures when the test is executed at a time that is
// very close to a day boundary, we make it deterministic by using a static
// reference date for all the time-based statistics.
const gReferenceTimeMs = new Date("2000-01-01T00:00:00").getTime();

// Returns a milliseconds value to use with nsILoginMetaInfo properties, falling
// approximately in the middle of the specified number of days before the
// reference time, where zero days indicates a time within the past 24 hours.
var daysBeforeMs = days => gReferenceTimeMs - (days + 0.5) * MS_PER_DAY;

/**
 * Contains metadata that will be attached to test logins in order to verify
 * that the statistics collection is working properly. Most properties of the
 * logins are initialized to the default test values already.
 *
 * If you update this data or any of the telemetry histograms it checks, you'll
 * probably need to update the expected statistics in the test below.
 */
const StatisticsTestData = [
  {
    timeLastUsed: daysBeforeMs(0),
  },
  {
    timeLastUsed: daysBeforeMs(1),
  },
  {
    timeLastUsed: daysBeforeMs(7),
    formSubmitURL: null,
    httpRealm: "The HTTP Realm",
  },
  {
    username: "",
    timeLastUsed: daysBeforeMs(7),
  },
  {
    username: "",
    timeLastUsed: daysBeforeMs(30),
  },
  {
    username: "",
    timeLastUsed: daysBeforeMs(31),
  },
  {
    timeLastUsed: daysBeforeMs(365),
  },
  {
    username: "",
    timeLastUsed: daysBeforeMs(366),
  },
  {
    // If the login was saved in the future, it is ignored for statistiscs.
    timeLastUsed: daysBeforeMs(-1),
  },
  {
    timeLastUsed: daysBeforeMs(1000),
  },
];

/**
 * Triggers the collection of those statistics that are not accumulated each
 * time an action is taken, but are a static snapshot of the current state.
 */
function triggerStatisticsCollection() {
  Services.obs.notifyObservers(null, "gather-telemetry", "" + gReferenceTimeMs);
}

/**
 * Tests the telemetry histogram with the given ID contains only the specified
 * non-zero ranges, expressed in the format { range1: value1, range2: value2 }.
 */
function testHistogram(histogramId, expectedNonZeroRanges) {
  let snapshot = Services.telemetry.getHistogramById(histogramId).snapshot();

  // Compute the actual ranges in the format { range1: value1, range2: value2 }.
  let actualNonZeroRanges = {};
  for (let [index, range] of snapshot.ranges.entries()) {
    let value = snapshot.counts[index];
    if (value > 0) {
      actualNonZeroRanges[range] = value;
    }
  }

  // These are stringified to visualize the differences between the values.
  do_print("Testing histogram: " + histogramId);
  do_check_eq(JSON.stringify(actualNonZeroRanges),
              JSON.stringify(expectedNonZeroRanges));
}

// Tests

/**
 * Enable local telemetry recording for the duration of the tests, and prepare
 * the test data that will be used by the following tests.
 */
add_task(function test_initialize() {
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  do_register_cleanup(function() {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });

  let uniqueNumber = 1;
  for (let loginModifications of StatisticsTestData) {
    loginModifications.hostname = `http://${uniqueNumber++}.example.com`;
    Services.logins.addLogin(TestData.formLogin(loginModifications));
  }
});

/**
 * Tests the collection of statistics related to login metadata.
 */
add_task(function test_logins_statistics() {
  // Repeat the operation twice to test that histograms are not accumulated.
  for (let repeating of [false, true]) {
    triggerStatisticsCollection();

    // Should record 1 in the bucket corresponding to the number of passwords.
    testHistogram("PWMGR_NUM_SAVED_PASSWORDS",
                  { 10: 1 });

    // Should record 1 in the bucket corresponding to the number of passwords.
    testHistogram("PWMGR_NUM_HTTPAUTH_PASSWORDS",
                  { 1: 1 });

    // For each saved login, should record 1 in the bucket corresponding to the
    // age in days since the login was last used.
    testHistogram("PWMGR_LOGIN_LAST_USED_DAYS",
                  { 0: 1, 1: 1, 7: 2, 29: 2, 356: 2, 750: 1 });

    // Should record the number of logins without a username in bucket 0, and
    // the number of logins with a username in bucket 1.
    testHistogram("PWMGR_USERNAME_PRESENT",
                  { 0: 4, 1: 6 });
  }
});

/**
 * Tests the collection of statistics related to hosts for which passowrd saving
 * has been explicitly disabled.
 */
add_task(function test_disabledHosts_statistics() {
  // Should record 1 in the bucket corresponding to the number of sites for
  // which password saving is disabled.
  Services.logins.setLoginSavingEnabled("http://www.example.com", false);
  triggerStatisticsCollection();
  testHistogram("PWMGR_BLOCKLIST_NUM_SITES", { 1: 1 });

  Services.logins.setLoginSavingEnabled("http://www.example.com", true);
  triggerStatisticsCollection();
  testHistogram("PWMGR_BLOCKLIST_NUM_SITES", { 0: 1 });
});

/**
 * Tests the collection of statistics related to general settings.
 */
add_task(function test_settings_statistics() {
  let oldRememberSignons = Services.prefs.getBoolPref("signon.rememberSignons");
  do_register_cleanup(function() {
    Services.prefs.setBoolPref("signon.rememberSignons", oldRememberSignons);
  });

  // Repeat the operation twice per value to test that histograms are reset.
  for (let remember of [false, true, false, true]) {
    // This change should be observed immediately by the login service.
    Services.prefs.setBoolPref("signon.rememberSignons", remember);

    triggerStatisticsCollection();

    // Should record 1 in either bucket 0 or bucket 1 based on the preference.
    testHistogram("PWMGR_SAVING_ENABLED", remember ? { 1: 1 } : { 0: 1 });
  }
});
