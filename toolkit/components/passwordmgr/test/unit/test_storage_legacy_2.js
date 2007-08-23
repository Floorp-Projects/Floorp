/*
 * Test suite for storage-Legacy.js -- exercises writing to on-disk storage.
 *
 * This test interfaces directly with the legacy login storage module,
 * bypassing the normal login manager usage.
 *
 */


function run_test() {

try {


/* ========== 0 ========== */
var testnum = 0;
var testdesc = "Initial connection to storage module"

var storage = Cc["@mozilla.org/login-manager/storage/legacy;1"].
              createInstance(Ci.nsILoginManagerStorage);
if (!storage)
    throw "Couldn't create storage instance.";


/* ========== 1 ========== */
testnum++;
var testdesc = "Create nsILoginInfo instances for testing with"

var dummyuser1 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser2 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser3 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);

dummyuser1.init( "http://dummyhost.mozilla.org", "", null,
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");

dummyuser2.init("http://dummyhost2.mozilla.org", "http://cgi.site.com", null,
    "dummydude2", "itsasecret2", "put_user2_here", "put_pw2_here");

dummyuser3.init("http://dummyhost2.mozilla.org", "http://cgi.site.com", null,
    "dummydude3", "itsasecret3", "put_user3_here", "put_pw3_here");


/* ========== 2 ========== */
testnum++;

testdesc = "[ensuring file doesn't exist]";
var filename="non-existant-file-"+Math.floor(Math.random() * 10000);
var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
file.initWithPath(OUTDIR);
file.append(filename);
var exists = file.exists();
if (exists) {
    // Go ahead and remove the file, so that this failure
    // doesn't repeat itself w/o intervention.
    file.remove(false);
    do_check_false(exists); // fail on purpose
}

testdesc = "Initialize with no existing file";
LoginTest.initStorage(storage, OUTDIR, filename, null, null, null);
LoginTest.checkStorageData(storage, [], []);

testdesc = "Add 1 disabled host only";
storage.setLoginSavingEnabled("http://www.nsa.gov", false);
LoginTest.checkStorageData(storage, ["http://www.nsa.gov"], []);

file.remove(false);



/* ========== 3 ========== */
testnum++;

testdesc = "Initialize with existing file (valid, but empty)";
LoginTest.initStorage(storage, INDIR, "signons-empty.txt",
                               OUTDIR, "output-01.txt");
LoginTest.checkStorageData(storage, [], []);

testdesc = "Add 1 disabled host only";
storage.setLoginSavingEnabled("http://www.nsa.gov", false);

LoginTest.checkStorageData(storage, ["http://www.nsa.gov"], []);

testdesc = "Remove disabled host only";
storage.setLoginSavingEnabled("http://www.nsa.gov", true);

LoginTest.checkStorageData(storage, [], []);


/* ========== 4 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
LoginTest.initStorage(storage, INDIR, "signons-empty.txt",
                               OUTDIR, "output-02.txt");

// Previous tests made sure we can write to an existing file, so now just
// tweak component to output elsewhere.
testdesc = "Add 1 login only";
storage.addLogin(dummyuser1);

testdesc = "[flush and reload for verification]";
LoginTest.initStorage(storage, OUTDIR, "output-02.txt", null, null);

testdesc = "Verify output-02.txt";
LoginTest.checkStorageData(storage, [], [dummyuser1]);


/* ========== 5 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
LoginTest.initStorage(storage, INDIR, "signons-empty.txt",
                               OUTDIR, "output-03.txt");

testdesc = "Add 1 disabled host only";
storage.setLoginSavingEnabled("http://www.nsa.gov", false);

testdesc = "[flush and reload for verification]";
LoginTest.initStorage(storage, OUTDIR, "output-03.txt", null, null);

testdesc = "Verify output-03.txt";
LoginTest.checkStorageData(storage, ["http://www.nsa.gov"], []);


/* ========== 6 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
LoginTest.initStorage(storage, INDIR, "signons-empty.txt",
                               OUTDIR, "output-04.txt");

testdesc = "Add 1 disabled host and 1 login";
storage.setLoginSavingEnabled("http://www.nsa.gov", false);
storage.addLogin(dummyuser1);

testdesc = "[flush and reload for verification]";
LoginTest.initStorage(storage, OUTDIR, "output-04.txt", null, null);

testdesc = "Verify output-04.txt";
LoginTest.checkStorageData(storage, ["http://www.nsa.gov"], [dummyuser1]);


/* ========== 7 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
LoginTest.initStorage(storage, INDIR, "signons-empty.txt",
                               OUTDIR, "output-03.txt");

testdesc = "Add 2 logins (to different hosts)";
storage.addLogin(dummyuser1);
storage.addLogin(dummyuser2);

testdesc = "[flush and reload for verification]";
LoginTest.initStorage(storage, OUTDIR, "output-03.txt", null, null);

testdesc = "Verify output-03.txt";
LoginTest.checkStorageData(storage, [], [dummyuser2, dummyuser1]);


/* ========== 8 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
LoginTest.initStorage(storage, INDIR, "signons-empty.txt",
                               OUTDIR, "output-04.txt");

testdesc = "Add 2 logins (to same host)";
storage.addLogin(dummyuser2);
storage.addLogin(dummyuser3);

testdesc = "[flush and reload for verification]";
LoginTest.initStorage(storage, OUTDIR, "output-04.txt", null, null);

testdesc = "Verify output-04.txt";
LoginTest.checkStorageData(storage, [], [dummyuser3, dummyuser2]);


/* ========== 9 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
LoginTest.initStorage(storage, INDIR, "signons-empty.txt",
                               OUTDIR, "output-05.txt");

testdesc = "Add 3 logins (2 to same host)";
storage.addLogin(dummyuser3);
storage.addLogin(dummyuser1);
storage.addLogin(dummyuser2);

testdesc = "[flush and reload for verification]";
LoginTest.initStorage(storage, OUTDIR, "output-05.txt", null, null);

testdesc = "Verify output-05.txt";
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3]);

// count dummyhost2 logins
do_check_eq(2, storage.countLogins("http://dummyhost2.mozilla.org", "", ""));
// count dummyhost logins
do_check_eq(1, storage.countLogins("http://dummyhost.mozilla.org",  "", ""));

// count dummyhost2 logins w/ specific formSubmitURL
do_check_eq(2, storage.countLogins("http://dummyhost2.mozilla.org", "http://cgi.site.com", ""));

/* ========== 10 ========== */
testnum++;

// test writing a file that already contained entries
// test overwriting a file early
// test permissions on output file (osx/unix)
// test starting with an invalid file, and a non-existant file.
// excercise case where we immediately write the data on init, for empty entries
// test writing a gazillion entries

} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

};
