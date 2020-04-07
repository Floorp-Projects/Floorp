/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getDBVersion(dbfile) {
  let dbConnection = Services.storage.openDatabase(dbfile);
  let version = dbConnection.schemaVersion;
  dbConnection.close();

  return version;
}

function indexExists(dbfile, indexname) {
  let dbConnection = Services.storage.openDatabase(dbfile);
  let result = dbConnection.indexExists(indexname);
  dbConnection.close();

  return result;
}

add_task(async function() {
  try {
    let testfile = do_get_file("data/cookies_v10.sqlite");
    let profileDir = do_get_profile();

    // Cleanup from any previous tests or failures.
    let destFile = profileDir.clone();
    destFile.append("cookies.sqlite");
    if (destFile.exists()) {
      destFile.remove(false);
    }

    testfile.copyTo(profileDir, "cookies.sqlite");
    Assert.equal(10, getDBVersion(destFile));

    Assert.ok(destFile.exists());

    // Check that the index exists
    Assert.ok(indexExists(destFile, "moz_basedomain"));

    // Do something that will cause the cookie service access and upgrade the
    // database.
    let cookies = Services.cookies.cookies;

    // Pretend that we're about to shut down, to tell the cookie manager
    // to clean up its connection with its database.
    Services.obs.notifyObservers(null, "profile-before-change");

    // check for upgraded schema.
    Assert.equal(11, getDBVersion(destFile));

    // Check that the index was deleted
    Assert.ok(!indexExists(destFile, "moz_basedomain"));
  } catch (e) {
    throw new Error(`FAILED: ${e}`);
  }
});
