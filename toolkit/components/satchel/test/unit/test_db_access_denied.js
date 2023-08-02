/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var bakFile;
var dbFile;

function run_test() {
  let testfile = do_get_file("formhistory_apitest.sqlite");
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  let destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists()) {
    destFile.remove(false);
  }

  bakFile = profileDir.clone();
  bakFile.append("formhistory.sqlite.corrupt");
  if (bakFile.exists()) {
    bakFile.remove(false);
  }

  dbFile = profileDir.clone();
  dbFile.append("formhistory.sqlite");

  testfile.copyTo(profileDir, "formhistory.sqlite");

  run_next_test();
}

add_test(async function initialize_database_in_readonly_results_in_db_reset() {
  // original permissions are 440, now set to not readable...
  dbFile.permissions = 0;

  // ...and reset them later for the next connection setup retry, which happens
  // after 3 retries (10 + 20 + 40) ms
  do_timeout(70, () => (dbFile.permissions = 440));

  // this establishes a connection, the first one will fail but after a few
  // retries we will have sufficiant permissions
  const numEntriesAfter = await FormHistory.count({});

  // original fixture data present
  Assert.equal(9, numEntriesAfter);

  // No backup has been created
  Assert.ok(!bakFile.exists(), "backup file does not exist");

  run_next_test();
});
