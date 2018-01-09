/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;

var iter;

function run_test() {
  do_test_pending();
  iter = next_test();
  iter.next();
}

function* next_test() {
  try {
  // ===== test init =====
    let testfile = do_get_file("formhistory_v3v4.sqlite");
    let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

    // Cleanup from any previous tests or failures.
    let destFile = profileDir.clone();
    destFile.append("formhistory.sqlite");
    if (destFile.exists()) {
      destFile.remove(false);
    }

    testfile.copyTo(profileDir, "formhistory.sqlite");
    Assert.equal(3, getDBVersion(testfile));

    // ===== 1 =====
    testnum++;

    destFile = profileDir.clone();
    destFile.append("formhistory.sqlite");
    let dbConnection = Services.storage.openUnsharedDatabase(destFile);

    // check for upgraded schema.
    Assert.equal(CURRENT_SCHEMA, FormHistory.schemaVersion);

    // Check that the index was added
    Assert.ok(dbConnection.tableExists("moz_deleted_formhistory"));
    dbConnection.close();

    // check that an entry still exists
    yield countEntries("name-A", "value-A",
                       function(num) {
                         Assert.ok(num > 0);
                         do_test_finished();
                       }
    );
  } catch (e) {
    throw new Error(`FAILED in test #${testnum} -- ${e}`);
  }
}
