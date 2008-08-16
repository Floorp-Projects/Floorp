/*
 * Test suite for storage-mozStorage.js -- expected initialization errors & fixes
 *                                         and database corruption.
 *
 * This test interfaces directly with the mozStorage login storage module,
 * bypassing the normal login manager usage.
 *
 */

function run_test() {

try {

// Disable test for now
return;


/* ========== 0 ========== */
var testnum = 0;
var testdesc = "Initial connection to storage module"

var storage = LoginTest.newMozStorage();
if (!storage)
throw "Couldn't create storage instance.";


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
 * Pass in a legacy file that has problems (bad header). On call to
 * getAllLogins should throw "Initialization failed". We replace the bad file
 * with a good one, and call getAllLogins again, which will attempt to import
 * again and should succeed because it has a good file now.
 */

/* ========== 2 ========== */
testnum++;
var testdesc = "Initialization, reinitialization, & importing"

var storage = LoginTest.newMozStorage();
// signons-00.txt has bad header; use signons.sqlite
LoginTest.initStorage(storage, INDIR, "signons-00.txt");
try {
    storage.getAllLogins({});
} catch (e) {
    var error = e;
}
LoginTest.checkExpectedError(/Initialization failed/, error);

// Since initWithFile will not replace the DB if not passed one, we can just
// call LoginTest.initStorage with with signons-06.txt (1 disabled, 1 login).
// storage is already defined, so this is acceptable use (a bit hacky though)
LoginTest.initStorage(storage, INDIR, "signons-06.txt");
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
var corruptDB = do_get_file("toolkit/components/passwordmgr/test/unit/data/" +
                            "corruptDB.sqlite");

corruptDB.copyTo(PROFDIR, filename)

// sanity check that the file copy worked
var cfile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
cfile.initWithPath(OUTDIR);
cfile.append(filename);
do_check_true(cfile.exists());

// will init mozStorage module with default filename.
var storage = LoginTest.newMozStorage();
LoginTest.initStorage(storage, null, null, OUTDIR, filename, null, true);
try {
    storage.getAllLogins({});
} catch (e) {
    var error = e;
}
LoginTest.checkExpectedError(/Initialization failed/, error);

// check that the backup file exists
var buFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
buFile.initWithPath(OUTDIR);
buFile.append(filename + ".corrupt");
do_check_true(buFile.exists());

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
