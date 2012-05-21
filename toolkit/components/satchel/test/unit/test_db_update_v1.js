/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Returns true if the timestamp is within 30 seconds of now.
function is_about_now(timestamp) {
  var delta = Math.abs(timestamp - 1000 * Date.now());
  var seconds = 30 * 1000000;
  return delta < seconds;
}

var testnum = 0;
var fh;
var timesUsed, firstUsed, lastUsed;

function run_test()
{
  try {

  // ===== test init =====
  var testfile = do_get_file("formhistory_v0.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_eq(0, getDBVersion(testfile));

  fh = Cc["@mozilla.org/satchel/form-history;1"].
       getService(Ci.nsIFormHistory2);


  // ===== 1 =====
  testnum++;
  // Check for expected contents.
  do_check_true(fh.entryExists("name-A", "value-A"));
  do_check_true(fh.entryExists("name-B", "value-B"));
  do_check_true(fh.entryExists("name-C", "value-C1"));
  do_check_true(fh.entryExists("name-C", "value-C2"));
  // check for upgraded schema.
  do_check_eq(CURRENT_SCHEMA, fh.DBConnection.schemaVersion);


  // ===== 2 =====
  testnum++;
  // Check that timestamps were created correctly.

  var query = "SELECT timesUsed, firstUsed, lastUsed " +
              "FROM moz_formhistory WHERE fieldname = 'name-A'";
  var stmt = fh.DBConnection.createStatement(query);
  stmt.executeStep();

  timesUsed = stmt.getInt32(0);
  firstUsed = stmt.getInt64(1);
  lastUsed  = stmt.getInt64(2);
  stmt.finalize();

  do_check_eq(1, timesUsed);
  do_check_true(firstUsed == lastUsed);
  // Upgraded entries timestamped 24 hours in the past.
  do_check_true(is_about_now(firstUsed + 24 * PR_HOURS));


  // ===== 3 =====
  testnum++;
  // Exercise adding and removing a name/value pair
  do_check_false(fh.entryExists("name-D", "value-D"));
  fh.addEntry("name-D", "value-D");
  do_check_true(fh.entryExists("name-D", "value-D"));
  fh.removeEntry("name-D", "value-D");
  do_check_false(fh.entryExists("name-D", "value-D"));


  // ===== 4 =====
  testnum++;
  // Add a new entry, check expected properties
  do_check_false(fh.entryExists("name-E", "value-E"));
  fh.addEntry("name-E", "value-E");
  do_check_true(fh.entryExists("name-E", "value-E"));

  query = "SELECT timesUsed, firstUsed, lastUsed " +
          "FROM moz_formhistory WHERE fieldname = 'name-E'";
  stmt = fh.DBConnection.createStatement(query);
  stmt.executeStep();

  timesUsed = stmt.getInt32(0);
  firstUsed = stmt.getInt64(1);
  lastUsed  = stmt.getInt64(2);
  stmt.finalize();

  do_check_eq(1, timesUsed);
  do_check_true(firstUsed == lastUsed);
  do_check_true(is_about_now(firstUsed));

  // The next test adds the entry again, and check to see that the lastUsed
  // field is updated. Unfortunately, on Windows PR_Now() is granular
  // (robarnold says usually 16.5ms, sometimes 10ms), so if we execute the
  // test too soon the timestamp will be the same! So, we'll wait a short
  // period of time to make sure the timestamp will differ.
  do_test_pending();
  do_timeout(50, delayed_test);

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}

function delayed_test() {
  try {

  // ===== 5 =====
  testnum++;
  // Add entry again, check for updated properties.
  do_check_true(fh.entryExists("name-E", "value-E"));
  fh.addEntry("name-E", "value-E");
  do_check_true(fh.entryExists("name-E", "value-E"));

  var query = "SELECT timesUsed, firstUsed, lastUsed " +
              "FROM moz_formhistory WHERE fieldname = 'name-E'";
  var stmt = fh.DBConnection.createStatement(query);
  stmt.executeStep();

  timesUsed = stmt.getInt32(0);
  var firstUsed2 = stmt.getInt64(1);
  var lastUsed2  = stmt.getInt64(2);
  stmt.finalize();

  do_check_eq(2, timesUsed);
  do_check_true(is_about_now(lastUsed2));

  do_check_true(firstUsed  == firstUsed2); //unchanged
  do_check_true(lastUsed   != lastUsed2);  //changed
  do_check_true(firstUsed2 != lastUsed2);

  do_test_finished();
  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
