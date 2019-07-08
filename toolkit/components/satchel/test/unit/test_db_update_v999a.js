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
    let testfile = do_get_file("formhistory_v999a.sqlite");
    let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

    // Cleanup from any previous tests or failures.
    let destFile = profileDir.clone();
    destFile.append("formhistory.sqlite");
    if (destFile.exists()) {
      destFile.remove(false);
    }

    testfile.copyTo(profileDir, "formhistory.sqlite");
    Assert.equal(999, getDBVersion(testfile));

    // ===== 1 =====
    testnum++;
    // Check for expected contents.
    Assert.ok((await promiseCountEntries(null, null)) > 0);
    Assert.equal(1, await promiseCountEntries("name-A", "value-A"));
    Assert.equal(1, await promiseCountEntries("name-B", "value-B"));
    Assert.equal(1, await promiseCountEntries("name-C", "value-C1"));
    Assert.equal(1, await promiseCountEntries("name-C", "value-C2"));
    Assert.equal(1, await promiseCountEntries("name-E", "value-E"));

    // check for downgraded schema.
    Assert.equal(CURRENT_SCHEMA, getDBVersion(destFile));

    // ===== 2 =====
    testnum++;
    // Exercise adding and removing a name/value pair
    Assert.equal(0, await promiseCountEntries("name-D", "value-D"));
    await promiseUpdateEntry("add", "name-D", "value-D");
    Assert.equal(1, await promiseCountEntries("name-D", "value-D"));
    await promiseUpdateEntry("remove", "name-D", "value-D");
    Assert.equal(0, await promiseCountEntries("name-D", "value-D"));
  } catch (e) {
    throw new Error(`FAILED in test #${testnum} -- ${e}`);
  }
});
