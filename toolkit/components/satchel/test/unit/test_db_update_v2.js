/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;
var fh;

function run_test()
{
  try {

  // ===== test init =====
  var testfile = do_get_file("formhistory_v1.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_eq(1, getDBVersion(testfile));

  fh = Cc["@mozilla.org/satchel/form-history;1"].
       getService(Ci.nsIFormHistory2);


  // ===== 1 =====
  testnum++;

  // Check that the index was added (which is all the v2 upgrade does)
  do_check_true(fh.DBConnection.indexExists("moz_formhistory_lastused_index"));
  // check for upgraded schema.
  do_check_eq(CURRENT_SCHEMA, fh.DBConnection.schemaVersion);
  // Check that old table was removed
  do_check_false(fh.DBConnection.tableExists("moz_dummy_table"));


  // ===== 2 =====
  testnum++;

  // Just sanity check for expected contents and that DB is working.
  do_check_true(fh.entryExists("name-A", "value-A"));
  do_check_false(fh.entryExists("name-B", "value-B"));
  fh.addEntry("name-B", "value-B");
  do_check_true(fh.entryExists("name-B", "value-B"));
  fh.removeEntry("name-B", "value-B");
  do_check_false(fh.entryExists("name-B", "value-B"));


  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
