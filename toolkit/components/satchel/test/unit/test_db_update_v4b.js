/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;
var fh;

function run_test()
{
  try {

  // ===== test init =====
  var testfile = do_get_file("formhistory_v3v4.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_eq(3, getDBVersion(testfile));

  fh = Cc["@mozilla.org/satchel/form-history;1"].
       getService(Ci.nsIFormHistory2);


  // ===== 1 =====
  testnum++;

  // Check that the index was added
  do_check_true(fh.DBConnection.tableExists("moz_deleted_formhistory"));
  // check for upgraded schema.
  do_check_eq(CURRENT_SCHEMA, fh.DBConnection.schemaVersion);
  // check that an entry still exists
  do_check_true(fh.entryExists("name-A", "value-A"));

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
