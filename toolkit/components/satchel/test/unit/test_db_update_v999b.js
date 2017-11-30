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

add_task(async function() {
  let testnum = 0;

  try {
    // ===== test init =====
    let testfile = do_get_file("formhistory_v999b.sqlite");
    let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

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
    Assert.equal(999, getDBVersion(destFile));

    // ===== 1 =====
    testnum++;

    // Open the DB, ensure that a backup of the corrupt DB is made.
    // DB init is done lazily so the DB shouldn't be created yet.
    Assert.ok(!bakFile.exists());
    // Doing any request to the DB should create it.
    await promiseCountEntries("", "");

    Assert.ok(bakFile.exists());
    bakFile.remove(false);

    // ===== 2 =====
    testnum++;
    // File should be empty
    Assert.ok(!await promiseCountEntries(null, null));
    Assert.equal(0, await promiseCountEntries("name-A", "value-A"));
    // check for current schema.
    Assert.equal(CURRENT_SCHEMA, getDBVersion(destFile));

    // ===== 3 =====
    testnum++;
    // Try adding an entry
    await promiseUpdateEntry("add", "name-A", "value-A");
    Assert.equal(1, await promiseCountEntries(null, null));
    Assert.equal(1, await promiseCountEntries("name-A", "value-A"));

    // ===== 4 =====
    testnum++;
    // Try removing an entry
    await promiseUpdateEntry("remove", "name-A", "value-A");
    Assert.equal(0, await promiseCountEntries(null, null));
    Assert.equal(0, await promiseCountEntries("name-A", "value-A"));
  } catch (e) {
    throw new Error(`FAILED in test #${testnum} -- ${e}`);
  }
});
