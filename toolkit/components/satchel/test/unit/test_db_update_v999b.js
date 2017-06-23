/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test uses a formhistory.sqlite with schema version set to 999 (a
 * future version). This exercies the code that allows using a future schema
 * version as long as the expected columns are present.
 *
 * Part A tests this when the columns do match, so the DB is used.
 * Part B tests this when the columns do *not* match, so the DB is reset.
 */

var iter = tests();

function run_test() {
  do_test_pending();
  iter.next();
}

function next_test() {
  iter.next();
}

function* tests() {
  let testnum = 0;

  try {
    // ===== test init =====
    let testfile = do_get_file("formhistory_v999b.sqlite");
    let profileDir = dirSvc.get("ProfD", Ci.nsIFile);

    // Cleanup from any previous tests or failures.
    let destFile = profileDir.clone();
    destFile.append("formhistory.sqlite");
    if (destFile.exists()) {
      destFile.remove(false);
    }

    let bakFile = profileDir.clone();
    bakFile.append("formhistory.sqlite.corrupt");
    if (bakFile.exists()) {
      bakFile.remove(false);
    }

    testfile.copyTo(profileDir, "formhistory.sqlite");
    do_check_eq(999, getDBVersion(testfile));

    let checkZero = function(num) { do_check_eq(num, 0); next_test(); };
    let checkOne = function(num) { do_check_eq(num, 1); next_test(); };

    // ===== 1 =====
    testnum++;

    // Open the DB, ensure that a backup of the corrupt DB is made.
    // DB init is done lazily so the DB shouldn't be created yet.
    do_check_false(bakFile.exists());
    // Doing any request to the DB should create it.
    yield countEntries("", "", next_test);

    do_check_true(bakFile.exists());
    bakFile.remove(false);

    // ===== 2 =====
    testnum++;
    // File should be empty
    yield countEntries(null, null, function(num) { do_check_false(num); next_test(); });
    yield countEntries("name-A", "value-A", checkZero);
    // check for current schema.
    do_check_eq(CURRENT_SCHEMA, FormHistory.schemaVersion);

    // ===== 3 =====
    testnum++;
    // Try adding an entry
    yield updateEntry("add", "name-A", "value-A", next_test);
    yield countEntries(null, null, checkOne);
    yield countEntries("name-A", "value-A", checkOne);

    // ===== 4 =====
    testnum++;
    // Try removing an entry
    yield updateEntry("remove", "name-A", "value-A", next_test);
    yield countEntries(null, null, checkZero);
    yield countEntries("name-A", "value-A", checkZero);
  } catch (e) {
    throw new Error(`FAILED in test #${testnum} -- ${e}`);
  }

  do_test_finished();
}
