/*
 * Test suite for storage-mozStorage.js -- expected initialization errors & fixes
 *                                         and database corruption.
 *
 * This test interfaces directly with the mozStorage login storage module,
 * bypassing the normal login manager usage.
 *
 */

const STORAGE_TYPE = "mozStorage";

function run_test() {

try {

var storage, testnum = 0;


/* ========== 1 ========== */
testnum++;
var testdesc = "Create nsILoginInfo instances for testing with"

var nsLoginInfo = new Components.Constructor(
                    "@mozilla.org/login-manager/loginInfo;1",
                    Components.interfaces.nsILoginInfo);
var testuser1 = new nsLoginInfo;
testuser1.init("http://dummyhost.mozilla.org", "", null,
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");

LoginTest.deleteFile(OUTDIR, "signons.sqlite");


/*
 * ---------------------- Initialization ----------------------
 * Pass in a legacy file that has problems (bad header). Initialization should
 * throw "Initialization failed". We replace the bad file with a good one,
 * and call getAllLogins again, which will attempt to import again and should
 * succeed because it has a good file now.
 */

/* ========== 2 ========== */
testnum++;
var testdesc = "Initialization, reinitialization, & importing"

var storage;
storage = LoginTest.initStorage(INDIR, "signons-00.txt", null, null, /Initialization failed/);

// Since initWithFile will not replace the DB if not passed one, we can just
// call LoginTest.initStorage with with signons-06.txt (1 disabled, 1 login).
// storage is already defined, so this is acceptable use (a bit hacky though)
storage = LoginTest.initStorage(INDIR, "signons-06.txt");
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);

LoginTest.deleteFile(OUTDIR, "signons.sqlite");


/*
 * ---------------------- DB Corruption ----------------------
 * Try to initialize with a corrupt database file. This should create a backup
 * file, then upon next use create a new database file.
 */

/* ========== 3 ========== */
testnum++;
var testdesc = "Corrupt database and backup"

var filename = "signons-c.sqlite";
// copy corrupt db to output directory
var corruptDB = do_get_file("data/corruptDB.sqlite");

var cfile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
cfile.initWithPath(OUTDIR);
cfile.append(filename);
if (cfile.exists())
    cfile.remove(false);

corruptDB.copyTo(PROFDIR, filename)

// sanity check that the file copy worked
do_check_true(cfile.exists());

// will init mozStorage module with corrupt database, init should fail
storage = LoginTest.reloadStorage(OUTDIR, filename, /Initialization failed/);

// check that the backup file exists
var buFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
buFile.initWithPath(OUTDIR);
buFile.append(filename + ".corrupt");
do_check_true(buFile.exists());

// check that the original corrupt file has been deleted
do_check_false(cfile.exists());

// initialize the storage module again
storage = LoginTest.reloadStorage(OUTDIR, filename);

// use the storage module again, should work now
storage.addLogin(testuser1);
LoginTest.checkStorageData(storage, [], [testuser1]);

// check the file exists
var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
file.initWithPath(OUTDIR);
file.append(filename);
do_check_true(file.exists());

LoginTest.deleteFile(OUTDIR, filename + ".corrupt");
LoginTest.deleteFile(OUTDIR, filename);


/* ========== end ========== */
} catch (e) {
 throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

};
