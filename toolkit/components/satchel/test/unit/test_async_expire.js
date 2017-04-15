/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var dbFile, oldSize;
var currentTestIndex = 0;

function triggerExpiration() {
  // We can't easily fake a "daily idle" event, so for testing purposes form
  // history listens for another notification to trigger an immediate
  // expiration.
  Services.obs.notifyObservers(null, "formhistory-expire-now");
}

var checkExists = function(num) { do_check_true(num > 0); next_test(); }
var checkNotExists = function(num) { do_check_true(!num); next_test(); }

var TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    do_check_eq(topic, "satchel-storage-changed");

    if (data == "formhistory-expireoldentries") {
      next_test();
    }
  }
};

function test_finished() {
  // Make sure we always reset prefs.
  if (Services.prefs.prefHasUserValue("browser.formfill.expire_days"))
    Services.prefs.clearUserPref("browser.formfill.expire_days");

  do_test_finished();
}

var iter = tests();

function run_test() {
  do_test_pending();
  iter.next();
}

function next_test() {
  iter.next();
}

function* tests() {
  Services.obs.addObserver(TestObserver, "satchel-storage-changed", true);

  // ===== test init =====
  var testfile = do_get_file("asyncformhistory_expire.sqlite");
  var profileDir = do_get_profile();

  // Cleanup from any previous tests or failures.
  dbFile = profileDir.clone();
  dbFile.append("formhistory.sqlite");
  if (dbFile.exists())
    dbFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_true(dbFile.exists());

  // We're going to clear this at the end, so it better have the default value now.
  do_check_false(Services.prefs.prefHasUserValue("browser.formfill.expire_days"));

  // Sanity check initial state
  yield countEntries(null, null, function(num) { do_check_eq(508, num); next_test(); });
  yield countEntries("name-A", "value-A", checkExists); // lastUsed == distant past
  yield countEntries("name-B", "value-B", checkExists); // lastUsed == distant future

  do_check_eq(CURRENT_SCHEMA, FormHistory.schemaVersion);

  // Add a new entry
  yield countEntries("name-C", "value-C", checkNotExists);
  yield addEntry("name-C", "value-C", next_test);
  yield countEntries("name-C", "value-C", checkExists);

  // Update some existing entries to have ages relative to when the test runs.
  var now = 1000 * Date.now();
  let updateLastUsed = function updateLastUsedFn(results, age) {
    let lastUsed = now - age * 24 * PR_HOURS;

    let changes = [ ];
    for (let r = 0; r < results.length; r++) {
      changes.push({ op: "update", lastUsed, guid: results[r].guid });
    }

    return changes;
  }

  let results = yield searchEntries(["guid"], { lastUsed: 181 }, iter);
  yield updateFormHistory(updateLastUsed(results, 181), next_test);

  results = yield searchEntries(["guid"], { lastUsed: 179 }, iter);
  yield updateFormHistory(updateLastUsed(results, 179), next_test);

  results = yield searchEntries(["guid"], { lastUsed: 31 }, iter);
  yield updateFormHistory(updateLastUsed(results, 31), next_test);

  results = yield searchEntries(["guid"], { lastUsed: 29 }, iter);
  yield updateFormHistory(updateLastUsed(results, 29), next_test);

  results = yield searchEntries(["guid"], { lastUsed: 9999 }, iter);
  yield updateFormHistory(updateLastUsed(results, 11), next_test);

  results = yield searchEntries(["guid"], { lastUsed: 9 }, iter);
  yield updateFormHistory(updateLastUsed(results, 9), next_test);

  yield countEntries("name-A", "value-A", checkExists);
  yield countEntries("181DaysOld", "foo", checkExists);
  yield countEntries("179DaysOld", "foo", checkExists);
  yield countEntries(null, null, function(num) { do_check_eq(509, num); next_test(); });

  // 2 entries are expected to expire.
  triggerExpiration();
  yield;

  yield countEntries("name-A", "value-A", checkNotExists);
  yield countEntries("181DaysOld", "foo", checkNotExists);
  yield countEntries("179DaysOld", "foo", checkExists);
  yield countEntries(null, null, function(num) { do_check_eq(507, num); next_test(); });

  // And again. No change expected.
  triggerExpiration();
  yield;

  yield countEntries(null, null, function(num) { do_check_eq(507, num); next_test(); });

  // Set formfill pref to 30 days.
  Services.prefs.setIntPref("browser.formfill.expire_days", 30);
  yield countEntries("179DaysOld", "foo", checkExists);
  yield countEntries("bar", "31days", checkExists);
  yield countEntries("bar", "29days", checkExists);
  yield countEntries(null, null, function(num) { do_check_eq(507, num); next_test(); });

  triggerExpiration();
  yield;

  yield countEntries("179DaysOld", "foo", checkNotExists);
  yield countEntries("bar", "31days", checkNotExists);
  yield countEntries("bar", "29days", checkExists);
  yield countEntries(null, null, function(num) { do_check_eq(505, num); next_test(); });

  // Set override pref to 10 days and expire. This expires a large batch of
  // entries, and should trigger a VACCUM to reduce file size.
  Services.prefs.setIntPref("browser.formfill.expire_days", 10);

  yield countEntries("bar", "29days", checkExists);
  yield countEntries("9DaysOld", "foo", checkExists);
  yield countEntries(null, null, function(num) { do_check_eq(505, num); next_test(); });

  triggerExpiration();
  yield;

  yield countEntries("bar", "29days", checkNotExists);
  yield countEntries("9DaysOld", "foo", checkExists);
  yield countEntries("name-B", "value-B", checkExists);
  yield countEntries("name-C", "value-C", checkExists);
  yield countEntries(null, null, function(num) { do_check_eq(3, num); next_test(); });

  test_finished();
}
