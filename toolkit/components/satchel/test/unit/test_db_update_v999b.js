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

function run_test()
{
  try {
  var testnum = 0;

  // ===== test init =====
  var testfile = do_get_file("formhistory_v999b.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  var bakFile = profileDir.clone();
  bakFile.append("formhistory.sqlite.corrupt");
  if (bakFile.exists())
    bakFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_eq(999, getDBVersion(testfile));

  // ===== 1 =====
  testnum++;
  // Open the DB, ensure that a backup of the corrupt DB is made.
  do_check_false(bakFile.exists());
  var fh = Cc["@mozilla.org/satchel/form-history;1"].
           getService(Ci.nsIFormHistory2);
  // DB init is done lazily so the DB shouldn't be created yet.
  do_check_false(bakFile.exists());
  // Doing any request to the DB should create it.
  fh.DBConnection;
  do_check_true(bakFile.exists());
  bakFile.remove(false);

  // ===== 2 =====
  testnum++;
  // File should be empty
  do_check_false(fh.hasEntries);
  do_check_false(fh.entryExists("name-A", "value-A"));
  // check for current schema.
  do_check_eq(CURRENT_SCHEMA, fh.DBConnection.schemaVersion);

  // ===== 3 =====
  testnum++;
  // Try adding an entry
  fh.addEntry("name-A", "value-A");
  do_check_true(fh.hasEntries);
  do_check_true(fh.entryExists("name-A", "value-A"));


  // ===== 4 =====
  testnum++;
  // Try removing an entry
  fh.removeEntry("name-A", "value-A");
  do_check_false(fh.hasEntries);
  do_check_false(fh.entryExists("name-A", "value-A"));

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
