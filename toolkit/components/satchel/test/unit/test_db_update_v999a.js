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
  try {
  var testnum = 0;

  // ===== test init =====
  var testfile = do_get_file("formhistory_v999a.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_eq(999, getDBVersion(testfile));

  let checkZero = function(num) { do_check_eq(num, 0); next_test(); }
  let checkOne = function(num) { do_check_eq(num, 1); next_test(); }

  // ===== 1 =====
  testnum++;
  // Check for expected contents.
  yield countEntries(null, null, function(num) { do_check_true(num > 0); next_test(); });
  yield countEntries("name-A", "value-A", checkOne);
  yield countEntries("name-B", "value-B", checkOne);
  yield countEntries("name-C", "value-C1", checkOne);
  yield countEntries("name-C", "value-C2", checkOne);
  yield countEntries("name-E", "value-E", checkOne);

  // check for downgraded schema.
  do_check_eq(CURRENT_SCHEMA, FormHistory.schemaVersion);

  // ===== 2 =====
  testnum++;
  // Exercise adding and removing a name/value pair
  yield countEntries("name-D", "value-D", checkZero);
  yield updateEntry("add", "name-D", "value-D", next_test);
  yield countEntries("name-D", "value-D", checkOne);
  yield updateEntry("remove", "name-D", "value-D", next_test);
  yield countEntries("name-D", "value-D", checkZero);

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }

  do_test_finished();
}
