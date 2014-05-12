/*
 * This test interfaces directly with the mozStorage password storage module,
 * bypassing the normal password manager usage.
 */


const ENCTYPE_BASE64 = 0;
const ENCTYPE_SDR = 1;

// Current schema version used by storage-mozStorage.js. This will need to be
// kept in sync with the version there (or else the tests fail).
const CURRENT_SCHEMA = 5;

LoginTest.copyFile = function (aLeafName)
{
  yield OS.File.copy(OS.Path.join(do_get_file("data").path, aLeafName),
                     OS.Path.join(OS.Constants.Path.profileDir, aLeafName));
};

LoginTest.openDB = function (aLeafName)
{
  var dbFile = new FileUtils.File(OS.Constants.Path.profileDir);
  dbFile.append(aLeafName);

  return Services.storage.openDatabase(dbFile);
};

LoginTest.deleteFile = function (pathname, filename)
{
  var file = new FileUtils.File(pathname);
  file.append(filename);

  // Suppress failures, this happens in the mozstorage tests on Windows
  // because the module may still be holding onto the DB. (We don't
  // have a way to explicitly shutdown/GC the module).
  try {
    if (file.exists())
      file.remove(false);
  } catch (e) {}
};

LoginTest.reloadStorage = function (aInputPathName, aInputFileName)
{
  var inputFile = null;
  if (aInputFileName) {
      var inputFile  = Cc["@mozilla.org/file/local;1"].
                       createInstance(Ci.nsILocalFile);
      inputFile.initWithPath(aInputPathName);
      inputFile.append(aInputFileName);
  }

  let storage = Cc["@mozilla.org/login-manager/storage/mozStorage;1"]
                  .createInstance(Ci.nsILoginManagerStorage);
  storage.QueryInterface(Ci.nsIInterfaceRequestor)
         .getInterface(Ci.nsIVariant)
         .initWithFile(inputFile);

  return storage;
};

LoginTest.checkStorageData = function (storage, ref_disabledHosts, ref_logins)
{
  LoginTest.assertLoginListsEqual(storage.getAllLogins(), ref_logins);
  LoginTest.assertDisabledHostsEqual(storage.getAllDisabledHosts(), ref_disabledHosts);
};

add_task(function test_execute()
{

const OUTDIR = OS.Constants.Path.profileDir;

try {

var isGUID = /^\{[0-9a-f\d]{8}-[0-9a-f\d]{4}-[0-9a-f\d]{4}-[0-9a-f\d]{4}-[0-9a-f\d]{12}\}$/;
function getGUIDforID(conn, id) {
    var stmt = conn.createStatement("SELECT guid from moz_logins WHERE id = " + id);
    stmt.executeStep();
    var guid = stmt.getString(0);
    stmt.finalize();
    return guid;
}

function getEncTypeForID(conn, id) {
    var stmt = conn.createStatement("SELECT encType from moz_logins WHERE id = " + id);
    stmt.executeStep();
    var encType = stmt.row.encType;
    stmt.finalize();
    return encType;
}

var storage;
var dbConnection;
var testnum = 0;
var testdesc = "Setup of nsLoginInfo test-users";
var nsLoginInfo = new Components.Constructor(
                    "@mozilla.org/login-manager/loginInfo;1",
                    Components.interfaces.nsILoginInfo);
do_check_true(nsLoginInfo != null);

var testuser1 = new nsLoginInfo;
testuser1.init("http://test.com", "http://test.com", null,
               "testuser1", "testpass1", "u1", "p1");
var testuser1B = new nsLoginInfo;
testuser1B.init("http://test.com", "http://test.com", null,
                "testuser1B", "testpass1B", "u1", "p1");
var testuser2 = new nsLoginInfo;
testuser2.init("http://test.org", "http://test.org", null,
               "testuser2", "testpass2", "u2", "p2");
var testuser3 = new nsLoginInfo;
testuser3.init("http://test.gov", "http://test.gov", null,
               "testuser3", "testpass3", "u3", "p3");
var testuser4 = new nsLoginInfo;
testuser4.init("http://test.gov", "http://test.gov", null,
               "testuser1", "testpass2", "u4", "p4");
var testuser5 = new nsLoginInfo;
testuser5.init("http://test.gov", "http://test.gov", null,
               "testuser2", "testpass1", "u5", "p5");


/* ========== 1 ========== */
testnum++;
testdesc = "Test downgrade from v999 storage"

yield LoginTest.copyFile("signons-v999.sqlite");
// Verify the schema version in the test file.
dbConnection = LoginTest.openDB("signons-v999.sqlite");
do_check_eq(999, dbConnection.schemaVersion);
dbConnection.close();

storage = LoginTest.reloadStorage(OUTDIR, "signons-v999.sqlite");
LoginTest.checkStorageData(storage, ["https://disabled.net"], [testuser1]);

// Check to make sure we downgraded the schema version.
dbConnection = LoginTest.openDB("signons-v999.sqlite");
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);
dbConnection.close();

LoginTest.deleteFile(OUTDIR, "signons-v999.sqlite");

/* ========== 2 ========== */
testnum++;
testdesc = "Test downgrade from incompat v999 storage"
// This file has a testuser999/testpass999, but is missing an expected column

var origFile = OS.Path.join(OUTDIR, "signons-v999-2.sqlite");
var failFile = OS.Path.join(OUTDIR, "signons-v999-2.sqlite.corrupt");

// Make sure we always start clean in a clean state.
yield LoginTest.copyFile("signons-v999-2.sqlite");
yield OS.File.remove(failFile);

Assert.throws(() => LoginTest.reloadStorage(OUTDIR, "signons-v999-2.sqlite"),
              /Initialization failed/);

// Check to ensure the DB file was renamed to .corrupt.
do_check_false(yield OS.File.exists(origFile));
do_check_true(yield OS.File.exists(failFile));

yield OS.File.remove(failFile);

/* ========== 3 ========== */
testnum++;
testdesc = "Test upgrade from v1->v2 storage"

yield LoginTest.copyFile("signons-v1.sqlite");
// Sanity check the test file.
dbConnection = LoginTest.openDB("signons-v1.sqlite");
do_check_eq(1, dbConnection.schemaVersion);
dbConnection.close();

storage = LoginTest.reloadStorage(OUTDIR, "signons-v1.sqlite");
LoginTest.checkStorageData(storage, ["https://disabled.net"], [testuser1, testuser2]);

// Check to see that we added a GUIDs to the logins.
dbConnection = LoginTest.openDB("signons-v1.sqlite");
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);
var guid = getGUIDforID(dbConnection, 1);
do_check_true(isGUID.test(guid));
guid = getGUIDforID(dbConnection, 2);
do_check_true(isGUID.test(guid));
dbConnection.close();

LoginTest.deleteFile(OUTDIR, "signons-v1.sqlite");

/* ========== 4 ========== */
testnum++;
testdesc = "Test upgrade v2->v1 storage";
// This is the case where a v2 DB has been accessed with v1 code, and now we
// are upgrading it again. Any logins added by the v1 code must be properly
// upgraded.

yield LoginTest.copyFile("signons-v1v2.sqlite");
// Sanity check the test file.
dbConnection = LoginTest.openDB("signons-v1v2.sqlite");
do_check_eq(1, dbConnection.schemaVersion);
dbConnection.close();

storage = LoginTest.reloadStorage(OUTDIR, "signons-v1v2.sqlite");
LoginTest.checkStorageData(storage, ["https://disabled.net"], [testuser1, testuser2, testuser3]);

// While we're here, try modifying a login, to ensure that doing so doesn't
// change the existing GUID.
storage.modifyLogin(testuser1, testuser1B);
LoginTest.checkStorageData(storage, ["https://disabled.net"], [testuser1B, testuser2, testuser3]);

// Check the GUIDs. Logins 1 and 2 should retain their original GUID, login 3
// should have one created (because it didn't have one previously).
dbConnection = LoginTest.openDB("signons-v1v2.sqlite");
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);
guid = getGUIDforID(dbConnection, 1);
do_check_eq("{655c7358-f1d6-6446-adab-53f98ac5d80f}", guid);
guid = getGUIDforID(dbConnection, 2);
do_check_eq("{13d9bfdc-572a-4d4e-9436-68e9803e84c1}", guid);
guid = getGUIDforID(dbConnection, 3);
do_check_true(isGUID.test(guid));
dbConnection.close();

LoginTest.deleteFile(OUTDIR, "signons-v1v2.sqlite");

/* ========== 5 ========== */
testnum++;
testdesc = "Test upgrade from v2->v3 storage"

yield LoginTest.copyFile("signons-v2.sqlite");
// Sanity check the test file.
dbConnection = LoginTest.openDB("signons-v2.sqlite");
do_check_eq(2, dbConnection.schemaVersion);

storage = LoginTest.reloadStorage(OUTDIR, "signons-v2.sqlite");

// Check to see that we added the correct encType to the logins.
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);
let encTypes = [ENCTYPE_BASE64, ENCTYPE_SDR, ENCTYPE_BASE64, ENCTYPE_BASE64];
for (let i = 0; i < encTypes.length; i++)
    do_check_eq(encTypes[i], getEncTypeForID(dbConnection, i + 1));
dbConnection.close();

// There are 4 logins, but 3 will be invalid because we can no longer decrypt
// base64-encoded items. (testuser1/4/5)
LoginTest.checkStorageData(storage, ["https://disabled.net"],
    [testuser2]);

LoginTest.deleteFile(OUTDIR, "signons-v2.sqlite");

/* ========== 6 ========== */
testnum++;
testdesc = "Test upgrade v3->v2 storage";
// This is the case where a v3 DB has been accessed with v2 code, and now we
// are upgrading it again. Any logins added by the v2 code must be properly
// upgraded.

yield LoginTest.copyFile("signons-v2v3.sqlite");
// Sanity check the test file.
dbConnection = LoginTest.openDB("signons-v2v3.sqlite");
do_check_eq(2, dbConnection.schemaVersion);
encTypes = [ENCTYPE_BASE64, ENCTYPE_SDR, ENCTYPE_BASE64, ENCTYPE_BASE64, null];
for (let i = 0; i < encTypes.length; i++)
    do_check_eq(encTypes[i], getEncTypeForID(dbConnection, i + 1));

// Reload storage, check that the new login now has encType=1, others untouched
storage = LoginTest.reloadStorage(OUTDIR, "signons-v2v3.sqlite");
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);

encTypes = [ENCTYPE_BASE64, ENCTYPE_SDR, ENCTYPE_BASE64, ENCTYPE_BASE64, ENCTYPE_SDR];
for (let i = 0; i < encTypes.length; i++)
    do_check_eq(encTypes[i], getEncTypeForID(dbConnection, i + 1));

// Sanity check that the data gets migrated
// There are 5 logins, but 3 will be invalid because we can no longer decrypt
// base64-encoded items. (testuser1/4/5). We no longer reencrypt with SDR.
LoginTest.checkStorageData(storage, ["https://disabled.net"], [testuser2, testuser3]);
encTypes = [ENCTYPE_BASE64, ENCTYPE_SDR, ENCTYPE_BASE64, ENCTYPE_BASE64, ENCTYPE_SDR];
for (let i = 0; i < encTypes.length; i++)
    do_check_eq(encTypes[i], getEncTypeForID(dbConnection, i + 1));
dbConnection.close();

LoginTest.deleteFile(OUTDIR, "signons-v2v3.sqlite");


/* ========== 7 ========== */
testnum++;
testdesc = "Test upgrade from v3->v4 storage"

yield LoginTest.copyFile("signons-v3.sqlite");
// Sanity check the test file.
dbConnection = LoginTest.openDB("signons-v3.sqlite");
do_check_eq(3, dbConnection.schemaVersion);

storage = LoginTest.reloadStorage(OUTDIR, "signons-v3.sqlite");
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);

// Check that timestamps and counts were initialized correctly
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);

var logins = storage.getAllLogins();
for (var i = 0; i < 2; i++) {
    do_check_true(logins[i] instanceof Ci.nsILoginMetaInfo);
    do_check_eq(1, logins[i].timesUsed);
    LoginTest.assertTimeIsAboutNow(logins[i].timeCreated);
    LoginTest.assertTimeIsAboutNow(logins[i].timeLastUsed);
    LoginTest.assertTimeIsAboutNow(logins[i].timePasswordChanged);
}

/* ========== 8 ========== */
testnum++;
testdesc = "Test upgrade from v3->v4->v3 storage"

yield LoginTest.copyFile("signons-v3v4.sqlite");
// Sanity check the test file.
dbConnection = LoginTest.openDB("signons-v3v4.sqlite");
do_check_eq(3, dbConnection.schemaVersion);

storage = LoginTest.reloadStorage(OUTDIR, "signons-v3v4.sqlite");
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);

// testuser1 already has timestamps, testuser2 does not.
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);

var logins = storage.getAllLogins();

var t1, t2;
if (logins[0].username == "testuser1") {
    t1 = logins[0];
    t2 = logins[1];
} else {
    t1 = logins[1];
    t2 = logins[0];
}

do_check_true(t1 instanceof Ci.nsILoginMetaInfo);
do_check_true(t2 instanceof Ci.nsILoginMetaInfo);

do_check_eq(9, t1.timesUsed);
do_check_eq(1262049951275, t1.timeCreated);
do_check_eq(1262049951275, t1.timeLastUsed);
do_check_eq(1262049951275, t1.timePasswordChanged);

do_check_eq(1, t2.timesUsed);
LoginTest.assertTimeIsAboutNow(t2.timeCreated);
LoginTest.assertTimeIsAboutNow(t2.timeLastUsed);
LoginTest.assertTimeIsAboutNow(t2.timePasswordChanged);


/* ========== 9 ========== */
testnum++;
testdesc = "Test upgrade from v4 storage"

yield LoginTest.copyFile("signons-v4.sqlite");
// Sanity check the test file.
dbConnection = LoginTest.openDB("signons-v4.sqlite");
do_check_eq(4, dbConnection.schemaVersion);
do_check_false(dbConnection.tableExists("moz_deleted_logins"));

storage = LoginTest.reloadStorage(OUTDIR, "signons-v4.sqlite");
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);
do_check_true(dbConnection.tableExists("moz_deleted_logins"));


/* ========== 10 ========== */
testnum++;
testdesc = "Test upgrade from v4->v5->v4 storage"

yield LoginTest.copyFile("signons-v4v5.sqlite");
// Sanity check the test file.
dbConnection = LoginTest.openDB("signons-v4v5.sqlite");
do_check_eq(4, dbConnection.schemaVersion);
do_check_true(dbConnection.tableExists("moz_deleted_logins"));

storage = LoginTest.reloadStorage(OUTDIR, "signons-v4v5.sqlite");
do_check_eq(CURRENT_SCHEMA, dbConnection.schemaVersion);
do_check_true(dbConnection.tableExists("moz_deleted_logins"));


/* ========== 11 ========== */
testnum++;
testdesc = "Create nsILoginInfo instances for testing with"

testuser1 = new nsLoginInfo;
testuser1.init("http://dummyhost.mozilla.org", "", null,
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");


/*
 * ---------------------- DB Corruption ----------------------
 * Try to initialize with a corrupt database file. This should create a backup
 * file, then upon next use create a new database file.
 */

/* ========== 12 ========== */
testnum++;
testdesc = "Corrupt database and backup"

const filename = "signons-c.sqlite";
const filepath = OS.Path.join(OS.Constants.Path.profileDir, filename);

yield OS.File.copy(do_get_file("data/corruptDB.sqlite").path, filepath);

// will init mozStorage module with corrupt database, init should fail
Assert.throws(
  () => LoginTest.reloadStorage(OS.Constants.Path.profileDir, filename),
  /Initialization failed/);

// check that the backup file exists
do_check_true(yield OS.File.exists(filepath + ".corrupt"));

// check that the original corrupt file has been deleted
do_check_false(yield OS.File.exists(filepath));

// initialize the storage module again
storage = LoginTest.reloadStorage(OS.Constants.Path.profileDir, filename);

// use the storage module again, should work now
storage.addLogin(testuser1);
LoginTest.checkStorageData(storage, [], [testuser1]);

// check the file exists
var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
file.initWithPath(OS.Constants.Path.profileDir);
file.append(filename);
do_check_true(file.exists());

LoginTest.deleteFile(OS.Constants.Path.profileDir, filename + ".corrupt");
LoginTest.deleteFile(OS.Constants.Path.profileDir, filename);

} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}

});
