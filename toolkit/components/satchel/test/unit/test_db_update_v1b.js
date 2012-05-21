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
  var testfile = do_get_file("formhistory_v0v1.sqlite");
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
  do_check_true(fh.entryExists("name-D", "value-D"));
  // check for upgraded schema.
  do_check_eq(CURRENT_SCHEMA, fh.DBConnection.schemaVersion);

  // ===== 2 =====
  testnum++;

  // The name-D entry was added by v0 code, so no timestamps were set. Make
  // sure the upgrade set timestamps on that entry.
  var query = "SELECT timesUsed, firstUsed, lastUsed " +
              "FROM moz_formhistory WHERE fieldname = 'name-D'";
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

  // Check to make sure the existing timestamps are unmodified.
  var query = "SELECT timesUsed, firstUsed, lastUsed " +
              "FROM moz_formhistory WHERE fieldname = 'name-A'";
  var stmt = fh.DBConnection.createStatement(query);
  stmt.executeStep();

  timesUsed = stmt.getInt32(0);
  firstUsed = stmt.getInt64(1);
  lastUsed  = stmt.getInt64(2);
  stmt.finalize();

  do_check_eq(1, timesUsed);
  do_check_eq(lastUsed,  1231984073012182);
  do_check_eq(firstUsed, 1231984073012182);

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
