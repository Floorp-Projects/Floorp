/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;
var fh;

function run_test()
{
  try {

  // ===== test init =====
  var testfile = do_get_file("formhistory_v2v3.sqlite");
  var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  var destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  do_check_eq(2, getDBVersion(testfile));

  fh = Cc["@mozilla.org/satchel/form-history;1"].
       getService(Ci.nsIFormHistory2);


  // ===== 1 =====
  testnum++;

  // Check that the index was added
  do_check_true(fh.DBConnection.indexExists("moz_formhistory_guid_index"));
  // check for upgraded schema.
  do_check_eq(CURRENT_SCHEMA, fh.DBConnection.schemaVersion);

  // Entry added by v3 code, has a GUID that shouldn't be changed.
  do_check_true(fh.entryExists("name-A", "value-A"));
  var guid = getGUIDforID(fh.DBConnection, 1);
  do_check_eq(guid, "dgdaRfzsTnOOZ7wK");

  // Entry added  by v2 code after a downgrade, GUID should be assigned on upgrade.
  do_check_true(fh.entryExists("name-B", "value-B"));
  guid = getGUIDforID(fh.DBConnection, 2);
  do_check_true(isGUID.test(guid));

  // Add a new entry and check that it gets a GUID
  do_check_false(fh.entryExists("name-C", "value-C"));
  fh.addEntry("name-C", "value-C");
  do_check_true(fh.entryExists("name-C", "value-C"));

  guid = getGUIDforID(fh.DBConnection, 3);
  do_check_true(isGUID.test(guid));

  } catch (e) {
    throw "FAILED in test #" + testnum + " -- " + e;
  }
}
