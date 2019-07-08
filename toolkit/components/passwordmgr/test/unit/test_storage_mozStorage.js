/**
 * This test interfaces directly with the mozStorage password storage module,
 * bypassing the normal password manager usage.
 */

/* eslint-disable no-var */

const ENCTYPE_BASE64 = 0;
const ENCTYPE_SDR = 1;
const PERMISSION_SAVE_LOGINS = "login-saving";

// Current schema version used by storage-mozStorage.js. This will need to be
// kept in sync with the version there (or else the tests fail).
const CURRENT_SCHEMA = 6;

async function copyFile(aLeafName) {
  await OS.File.copy(
    OS.Path.join(do_get_file("data").path, aLeafName),
    OS.Path.join(OS.Constants.Path.profileDir, aLeafName)
  );
}

function openDB(aLeafName) {
  let dbFile = new FileUtils.File(OS.Constants.Path.profileDir);
  dbFile.append(aLeafName);

  return Services.storage.openDatabase(dbFile);
}

function deleteFile(pathname, filename) {
  let file = new FileUtils.File(pathname);
  file.append(filename);

  // Suppress failures, this happens in the mozstorage tests on Windows
  // because the module may still be holding onto the DB. (We don't
  // have a way to explicitly shutdown/GC the module).
  try {
    if (file.exists()) {
      file.remove(false);
    }
  } catch (e) {}
}

function reloadStorage(aInputPathName, aInputFileName) {
  let inputFile = null;
  if (aInputFileName) {
    inputFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    inputFile.initWithPath(aInputPathName);
    inputFile.append(aInputFileName);
  }

  let storage = Cc[
    "@mozilla.org/login-manager/storage/mozStorage;1"
  ].createInstance(Ci.nsILoginManagerStorage);
  storage
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIVariant)
    .initWithFile(inputFile);

  return storage;
}

function checkStorageData(storage, ref_disabledHosts, ref_logins) {
  LoginTestUtils.assertLoginListsEqual(storage.getAllLogins(), ref_logins);
  LoginTestUtils.assertDisabledHostsEqual(
    getAllDisabledHostsFromPermissionManager(),
    ref_disabledHosts
  );
}

function getAllDisabledHostsFromPermissionManager() {
  let disabledHosts = [];
  for (let perm of Services.perms.enumerator) {
    if (
      perm.type == PERMISSION_SAVE_LOGINS &&
      perm.capability == Services.perms.DENY_ACTION
    ) {
      disabledHosts.push(perm.principal.URI.prePath);
    }
  }

  return disabledHosts;
}

function setLoginSavingEnabled(origin, enabled) {
  let uri = Services.io.newURI(origin);

  if (enabled) {
    Services.perms.remove(uri, PERMISSION_SAVE_LOGINS);
  } else {
    Services.perms.add(uri, PERMISSION_SAVE_LOGINS, Services.perms.DENY_ACTION);
  }
}

add_task(async function test_execute() {
  const OUTDIR = OS.Constants.Path.profileDir;
  let testnum = 0;
  let testdesc = "Setup of nsLoginInfo test-users";

  try {
    var isGUID = /^\{[0-9a-f\d]{8}-[0-9a-f\d]{4}-[0-9a-f\d]{4}-[0-9a-f\d]{4}-[0-9a-f\d]{12}\}$/;
    function getGUIDforID(conn, id) {
      let stmt = conn.createStatement(
        "SELECT guid from moz_logins WHERE id = " + id
      );
      stmt.executeStep();
      let guid = stmt.getString(0);
      stmt.finalize();
      return guid;
    }

    function getEncTypeForID(conn, id) {
      let stmt = conn.createStatement(
        "SELECT encType from moz_logins WHERE id = " + id
      );
      stmt.executeStep();
      let encType = stmt.row.encType;
      stmt.finalize();
      return encType;
    }

    function getAllDisabledHostsFromMozStorage(conn) {
      let disabledHosts = [];
      let stmt = conn.createStatement("SELECT hostname from moz_disabledHosts");

      while (stmt.executeStep()) {
        disabledHosts.push(stmt.row.hostname);
      }

      return disabledHosts;
    }

    var storage;
    var dbConnection;
    var nsLoginInfo = new Components.Constructor(
      "@mozilla.org/login-manager/loginInfo;1",
      Ci.nsILoginInfo
    );
    Assert.ok(nsLoginInfo != null);

    var testuser1 = new nsLoginInfo();
    testuser1.init(
      "http://test.com",
      "http://test.com",
      null,
      "testuser1",
      "testpass1",
      "u1",
      "p1"
    );
    var testuser1B = new nsLoginInfo();
    testuser1B.init(
      "http://test.com",
      "http://test.com",
      null,
      "testuser1B",
      "testpass1B",
      "u1",
      "p1"
    );
    var testuser2 = new nsLoginInfo();
    testuser2.init(
      "http://test.org",
      "http://test.org",
      null,
      "testuser2",
      "testpass2",
      "u2",
      "p2"
    );
    var testuser3 = new nsLoginInfo();
    testuser3.init(
      "http://test.gov",
      "http://test.gov",
      null,
      "testuser3",
      "testpass3",
      "u3",
      "p3"
    );
    var testuser4 = new nsLoginInfo();
    testuser4.init(
      "http://test.gov",
      "http://test.gov",
      null,
      "testuser1",
      "testpass2",
      "u4",
      "p4"
    );
    var testuser5 = new nsLoginInfo();
    testuser5.init(
      "http://test.gov",
      "http://test.gov",
      null,
      "testuser2",
      "testpass1",
      "u5",
      "p5"
    );

    /* ========== 1 ========== */
    testnum++;
    testdesc = "Test downgrade from v999 storage";

    await copyFile("signons-v999.sqlite");
    // Verify the schema version in the test file.
    dbConnection = openDB("signons-v999.sqlite");
    Assert.equal(999, dbConnection.schemaVersion);
    dbConnection.close();

    storage = reloadStorage(OUTDIR, "signons-v999.sqlite");
    setLoginSavingEnabled("https://disabled.net", false);
    checkStorageData(storage, ["https://disabled.net"], [testuser1]);

    // Check to make sure we downgraded the schema version.
    dbConnection = openDB("signons-v999.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);
    dbConnection.close();

    deleteFile(OUTDIR, "signons-v999.sqlite");

    /* ========== 2 ========== */
    testnum++;
    testdesc = "Test downgrade from incompat v999 storage";
    // This file has a testuser999/testpass999, but is missing an expected column

    var origFile = OS.Path.join(OUTDIR, "signons-v999-2.sqlite");
    var failFile = OS.Path.join(OUTDIR, "signons-v999-2.sqlite.corrupt");

    // Make sure we always start clean in a clean state.
    await copyFile("signons-v999-2.sqlite");
    await OS.File.remove(failFile);

    Assert.throws(
      () => reloadStorage(OUTDIR, "signons-v999-2.sqlite"),
      /Initialization failed/
    );

    // Check to ensure the DB file was renamed to .corrupt.
    Assert.equal(false, await OS.File.exists(origFile));
    Assert.ok(await OS.File.exists(failFile));

    await OS.File.remove(failFile);

    /* ========== 3 ========== */
    testnum++;
    testdesc = "Test upgrade from v1->v2 storage";

    await copyFile("signons-v1.sqlite");
    // Sanity check the test file.
    dbConnection = openDB("signons-v1.sqlite");
    Assert.equal(1, dbConnection.schemaVersion);
    dbConnection.close();

    storage = reloadStorage(OUTDIR, "signons-v1.sqlite");
    checkStorageData(storage, ["https://disabled.net"], [testuser1, testuser2]);

    // Check to see that we added a GUIDs to the logins.
    dbConnection = openDB("signons-v1.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);
    var guid = getGUIDforID(dbConnection, 1);
    Assert.ok(isGUID.test(guid));
    guid = getGUIDforID(dbConnection, 2);
    Assert.ok(isGUID.test(guid));
    dbConnection.close();

    deleteFile(OUTDIR, "signons-v1.sqlite");

    /* ========== 4 ========== */
    testnum++;
    testdesc = "Test upgrade v2->v1 storage";
    // This is the case where a v2 DB has been accessed with v1 code, and now we
    // are upgrading it again. Any logins added by the v1 code must be properly
    // upgraded.

    await copyFile("signons-v1v2.sqlite");
    // Sanity check the test file.
    dbConnection = openDB("signons-v1v2.sqlite");
    Assert.equal(1, dbConnection.schemaVersion);
    dbConnection.close();

    storage = reloadStorage(OUTDIR, "signons-v1v2.sqlite");
    checkStorageData(
      storage,
      ["https://disabled.net"],
      [testuser1, testuser2, testuser3]
    );

    // While we're here, try modifying a login, to ensure that doing so doesn't
    // change the existing GUID.
    storage.modifyLogin(testuser1, testuser1B);
    checkStorageData(
      storage,
      ["https://disabled.net"],
      [testuser1B, testuser2, testuser3]
    );

    // Check the GUIDs. Logins 1 and 2 should retain their original GUID, login 3
    // should have one created (because it didn't have one previously).
    dbConnection = openDB("signons-v1v2.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);
    guid = getGUIDforID(dbConnection, 1);
    Assert.equal("{655c7358-f1d6-6446-adab-53f98ac5d80f}", guid);
    guid = getGUIDforID(dbConnection, 2);
    Assert.equal("{13d9bfdc-572a-4d4e-9436-68e9803e84c1}", guid);
    guid = getGUIDforID(dbConnection, 3);
    Assert.ok(isGUID.test(guid));
    dbConnection.close();

    deleteFile(OUTDIR, "signons-v1v2.sqlite");

    /* ========== 5 ========== */
    testnum++;
    testdesc = "Test upgrade from v2->v3 storage";

    await copyFile("signons-v2.sqlite");
    // Sanity check the test file.
    dbConnection = openDB("signons-v2.sqlite");
    Assert.equal(2, dbConnection.schemaVersion);

    storage = reloadStorage(OUTDIR, "signons-v2.sqlite");

    // Check to see that we added the correct encType to the logins.
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);
    var encTypes = [
      ENCTYPE_BASE64,
      ENCTYPE_SDR,
      ENCTYPE_BASE64,
      ENCTYPE_BASE64,
    ];
    for (let i = 0; i < encTypes.length; i++) {
      Assert.equal(encTypes[i], getEncTypeForID(dbConnection, i + 1));
    }
    dbConnection.close();

    // There are 4 logins, but 3 will be invalid because we can no longer decrypt
    // base64-encoded items. (testuser1/4/5)
    checkStorageData(storage, ["https://disabled.net"], [testuser2]);

    deleteFile(OUTDIR, "signons-v2.sqlite");

    /* ========== 6 ========== */
    testnum++;
    testdesc = "Test upgrade v3->v2 storage";
    // This is the case where a v3 DB has been accessed with v2 code, and now we
    // are upgrading it again. Any logins added by the v2 code must be properly
    // upgraded.

    await copyFile("signons-v2v3.sqlite");
    // Sanity check the test file.
    dbConnection = openDB("signons-v2v3.sqlite");
    Assert.equal(2, dbConnection.schemaVersion);
    encTypes = [
      ENCTYPE_BASE64,
      ENCTYPE_SDR,
      ENCTYPE_BASE64,
      ENCTYPE_BASE64,
      null,
    ];
    for (let i = 0; i < encTypes.length; i++) {
      Assert.equal(encTypes[i], getEncTypeForID(dbConnection, i + 1));
    }

    // Reload storage, check that the new login now has encType=1, others untouched
    storage = reloadStorage(OUTDIR, "signons-v2v3.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);

    encTypes = [
      ENCTYPE_BASE64,
      ENCTYPE_SDR,
      ENCTYPE_BASE64,
      ENCTYPE_BASE64,
      ENCTYPE_SDR,
    ];
    for (let i = 0; i < encTypes.length; i++) {
      Assert.equal(encTypes[i], getEncTypeForID(dbConnection, i + 1));
    }

    // Sanity check that the data gets migrated
    // There are 5 logins, but 3 will be invalid because we can no longer decrypt
    // base64-encoded items. (testuser1/4/5). We no longer reencrypt with SDR.
    checkStorageData(storage, ["https://disabled.net"], [testuser2, testuser3]);
    encTypes = [
      ENCTYPE_BASE64,
      ENCTYPE_SDR,
      ENCTYPE_BASE64,
      ENCTYPE_BASE64,
      ENCTYPE_SDR,
    ];
    for (let i = 0; i < encTypes.length; i++) {
      Assert.equal(encTypes[i], getEncTypeForID(dbConnection, i + 1));
    }
    dbConnection.close();

    deleteFile(OUTDIR, "signons-v2v3.sqlite");

    /* ========== 7 ========== */
    testnum++;
    testdesc = "Test upgrade from v3->v4 storage";

    await copyFile("signons-v3.sqlite");
    // Sanity check the test file.
    dbConnection = openDB("signons-v3.sqlite");
    Assert.equal(3, dbConnection.schemaVersion);

    storage = reloadStorage(OUTDIR, "signons-v3.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);

    // Remove old entry from permission manager.
    setLoginSavingEnabled("https://disabled.net", true);

    // Check that timestamps and counts were initialized correctly
    checkStorageData(storage, [], [testuser1, testuser2]);

    var logins = storage.getAllLogins();
    for (var i = 0; i < 2; i++) {
      Assert.ok(logins[i] instanceof Ci.nsILoginMetaInfo);
      Assert.equal(1, logins[i].timesUsed);
      LoginTestUtils.assertTimeIsAboutNow(logins[i].timeCreated);
      LoginTestUtils.assertTimeIsAboutNow(logins[i].timeLastUsed);
      LoginTestUtils.assertTimeIsAboutNow(logins[i].timePasswordChanged);
    }

    /* ========== 8 ========== */
    testnum++;
    testdesc = "Test upgrade from v3->v4->v3 storage";

    await copyFile("signons-v3v4.sqlite");
    // Sanity check the test file.
    dbConnection = openDB("signons-v3v4.sqlite");
    Assert.equal(3, dbConnection.schemaVersion);

    storage = reloadStorage(OUTDIR, "signons-v3v4.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);

    // testuser1 already has timestamps, testuser2 does not.
    checkStorageData(storage, [], [testuser1, testuser2]);

    logins = storage.getAllLogins();

    var t1, t2;
    if (logins[0].username == "testuser1") {
      t1 = logins[0];
      t2 = logins[1];
    } else {
      t1 = logins[1];
      t2 = logins[0];
    }

    Assert.ok(t1 instanceof Ci.nsILoginMetaInfo);
    Assert.ok(t2 instanceof Ci.nsILoginMetaInfo);

    Assert.equal(9, t1.timesUsed);
    Assert.equal(1262049951275, t1.timeCreated);
    Assert.equal(1262049951275, t1.timeLastUsed);
    Assert.equal(1262049951275, t1.timePasswordChanged);

    Assert.equal(1, t2.timesUsed);
    LoginTestUtils.assertTimeIsAboutNow(t2.timeCreated);
    LoginTestUtils.assertTimeIsAboutNow(t2.timeLastUsed);
    LoginTestUtils.assertTimeIsAboutNow(t2.timePasswordChanged);

    /* ========== 9 ========== */
    testnum++;
    testdesc = "Test upgrade from v4 storage";

    await copyFile("signons-v4.sqlite");
    // Sanity check the test file.
    dbConnection = openDB("signons-v4.sqlite");
    Assert.equal(4, dbConnection.schemaVersion);
    Assert.ok(!dbConnection.tableExists("moz_deleted_logins"));

    storage = reloadStorage(OUTDIR, "signons-v4.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);
    Assert.ok(dbConnection.tableExists("moz_deleted_logins"));

    /* ========== 10 ========== */
    testnum++;
    testdesc = "Test upgrade from v4->v5->v4 storage";

    await copyFile("signons-v4v5.sqlite");
    // Sanity check the test file.
    dbConnection = openDB("signons-v4v5.sqlite");
    Assert.equal(4, dbConnection.schemaVersion);
    Assert.ok(dbConnection.tableExists("moz_deleted_logins"));

    storage = reloadStorage(OUTDIR, "signons-v4v5.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);
    Assert.ok(dbConnection.tableExists("moz_deleted_logins"));

    /* ========== 11 ========== */
    testnum++;
    testdesc = "Test upgrade from v5->v6 storage";

    await copyFile("signons-v5v6.sqlite");

    // Sanity check the test file.
    dbConnection = openDB("signons-v5v6.sqlite");
    Assert.equal(5, dbConnection.schemaVersion);
    Assert.ok(dbConnection.tableExists("moz_disabledHosts"));

    // Initial disabled hosts inside signons-v5v6.sqlite
    var disabledHosts = [
      "http://disabled1.example.com",
      "http://大.net",
      "http://xn--19g.com",
    ];

    LoginTestUtils.assertDisabledHostsEqual(
      disabledHosts,
      getAllDisabledHostsFromMozStorage(dbConnection)
    );

    // Reload storage
    storage = reloadStorage(OUTDIR, "signons-v5v6.sqlite");
    Assert.equal(CURRENT_SCHEMA, dbConnection.schemaVersion);

    // moz_disabledHosts should now be empty after migration.
    LoginTestUtils.assertDisabledHostsEqual(
      [],
      getAllDisabledHostsFromMozStorage(dbConnection)
    );

    // Get all the other hosts currently saved in the permission manager.
    let hostsInPermissionManager = getAllDisabledHostsFromPermissionManager();

    // Converted to punycode
    disabledHosts = [
      "http://disabled1.example.com",
      "http://xn--pss.net",
      "http://xn--19g.com",
    ];

    // All disabledHosts should have migrated to the permission manager
    LoginTestUtils.assertDisabledHostsEqual(
      disabledHosts,
      hostsInPermissionManager
    );

    // Remove all disabled hosts from the permission manager before test ends
    for (let host of disabledHosts) {
      setLoginSavingEnabled(host, true);
    }

    /* ========== 12 ========== */
    testnum++;
    testdesc = "Create nsILoginInfo instances for testing with";

    testuser1 = new nsLoginInfo();
    testuser1.init(
      "http://dummyhost.mozilla.org",
      "",
      null,
      "dummydude",
      "itsasecret",
      "put_user_here",
      "put_pw_here"
    );

    /*
     * ---------------------- DB Corruption ----------------------
     * Try to initialize with a corrupt database file. This should create a backup
     * file, then upon next use create a new database file.
     */

    /* ========== 13 ========== */
    testnum++;
    testdesc = "Corrupt database and backup";

    const filename = "signons-c.sqlite";
    const filepath = OS.Path.join(OS.Constants.Path.profileDir, filename);

    await OS.File.copy(do_get_file("data/corruptDB.sqlite").path, filepath);

    // will init mozStorage module with corrupt database, init should fail
    Assert.throws(
      () => reloadStorage(OS.Constants.Path.profileDir, filename),
      /Initialization failed/
    );

    // check that the backup file exists
    Assert.ok(await OS.File.exists(filepath + ".corrupt"));

    // check that the original corrupt file has been deleted
    Assert.equal(false, await OS.File.exists(filepath));

    // initialize the storage module again
    storage = reloadStorage(OS.Constants.Path.profileDir, filename);

    // use the storage module again, should work now
    storage.addLogin(testuser1);
    checkStorageData(storage, [], [testuser1]);

    // check the file exists
    var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(OS.Constants.Path.profileDir);
    file.append(filename);
    Assert.ok(file.exists());

    deleteFile(OS.Constants.Path.profileDir, filename + ".corrupt");
    deleteFile(OS.Constants.Path.profileDir, filename);
  } catch (e) {
    throw new Error(
      "FAILED in test #" + testnum + " -- " + testdesc + ": " + e
    );
  }
});
