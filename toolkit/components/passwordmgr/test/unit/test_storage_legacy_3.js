/*
 * Test suite for storage-Legacy.js -- various bug fixes.
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

dummyuser1.init("http://dummyhost.mozilla.org", "", null,
    "testuser1", "testpass1", "put_user_here", "put_pw_here");

dummyuser2.init("http://dummyhost2.mozilla.org", "", null,
    "testuser2", "testpass2", "put_user2_here", "put_pw2_here");

dummyuser3.init("http://dummyhost2.mozilla.org", "", null,
    "testuser3", "testpass3", "put_user3_here", "put_pw3_here");



/*
 * ---------------------- Bug 380961 ----------------------
 * Need to support decoding the mime64-obscured format still
 * used by SeaMonkey.
 */


/* ========== 2 ========== */
testnum++;

testdesc = "checking import of mime64-obscured entries"
LoginTest.initStorage(storage, INDIR, "signons-380961-1.txt",
                               OUTDIR, "output-380961-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1]);

testdesc = "[flush and reload for verification]"
LoginTest.initStorage(storage, OUTDIR, "output-380961-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1]);

/* ========== 3 ========== */
testnum++;

testdesc = "testing import of multiple mime-64 entries for a host"
LoginTest.initStorage(storage, INDIR, "signons-380961-2.txt",
                               OUTDIR, "output-380961-2.txt");
LoginTest.checkStorageData(storage, [], [dummyuser2, dummyuser3]);

testdesc = "[flush and reload for verification]"
LoginTest.initStorage(storage, OUTDIR, "output-380961-2.txt");
LoginTest.checkStorageData(storage, [], [dummyuser2, dummyuser3]);

/* ========== 4 ========== */
testnum++;

testdesc = "testing import of mixed encrypted and mime-64 entries."
LoginTest.initStorage(storage, INDIR, "signons-380961-3.txt",
                               OUTDIR, "output-380961-3.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3]);

testdesc = "[flush and reload for verification]"
LoginTest.initStorage(storage, OUTDIR, "output-380961-3.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3]);


/*
 * ---------------------- Bug 381262 ----------------------
 * The SecretDecoderRing can't handle UCS2, failure to
 * convert to UTF8 garbles the result.
 *
 * Note: dump()ing to the console on OS X (at least) outputs
 * garbage, whereas the "bad" UCS2 looks ok!
 */

/* ========== 5 ========== */
testnum++;

testdesc = "initializing login with non-ASCII data."
var dummyuser4 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);

dummyuser4.hostname      = "https://site.org";
dummyuser4.username      = String.fromCharCode(
                            355, 277, 349, 357, 533, 537, 101, 345, 185);
                            // "testuser1" using similar-looking glyphs
dummyuser4.usernameField = "username";
dummyuser4.password      = "testpa" + String.fromCharCode(223) + "1";
                            // "ss" replaced with German eszett.
dummyuser4.passwordField = "password";
dummyuser4.formSubmitURL = "https://site.org";
dummyuser4.httpRealm     = null;


/* ========== 6 ========== */
testnum++;

testdesc = "testing import of non-ascii username and password."
LoginTest.initStorage(storage, INDIR, "signons-381262.txt",
                               OUTDIR, "output-381262-1.txt");
var logins = storage.getAllLogins({});
LoginTest.checkStorageData(storage, [], [dummyuser4]);

testdesc = "[flush and reload for verification]"
LoginTest.initStorage(storage, OUTDIR, "output-381262-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser4]);


/* ========== 7 ========== */
testnum++;

testdesc = "testing storage of non-ascii username and password."
LoginTest.initStorage(storage, INDIR, "signons-empty.txt",
                               OUTDIR, "output-381262-2.txt");
LoginTest.checkStorageData(storage, [], []);
storage.addLogin(dummyuser4);
LoginTest.checkStorageData(storage, [], [dummyuser4]);

testdesc = "[flush and reload for verification]"
LoginTest.initStorage(storage, OUTDIR, "output-381262-2.txt");
logins = storage.getAllLogins({});
LoginTest.checkStorageData(storage, [], [dummyuser4]);


/*
 * ---------------------- Bug 400751 ----------------------
 * Migrating from existing mime64 encoded format causes
 * errors in storage legacy's code
 */


/* ========== 8 ========== */
testnum++;

testdesc = "checking double reading of mime64-obscured entries";
LoginTest.initStorage(storage, INDIR, "signons-380961-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1]);

testdesc = "checking double reading of mime64-obscured entries part 2";
LoginTest.checkStorageData(storage, [], [dummyuser1]);

/* ========== 9 ========== */
testnum++;

testdesc = "checking correct storage of mime64 converted entries";
LoginTest.initStorage(storage, INDIR, "signons-380961-1.txt",
                               OUTDIR, "output-400751-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1]);
LoginTest.checkStorageData(storage, [], [dummyuser1]);
storage.addLogin(dummyuser2); // trigger a write
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2]);

testdesc = "[flush and reload for verification]";
LoginTest.initStorage(storage, OUTDIR, "output-400751-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2]);

} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

};
