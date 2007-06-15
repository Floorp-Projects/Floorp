/*
 * Test suite for storage-Legacy.js -- various bug fixes.
 *
 * This test interfaces directly with the legacy login storage module,
 * bypassing the normal login manager usage.
 *
 */

var storage;

var DIR_SERVICE = new Components.Constructor(
                    "@mozilla.org/file/directory_service;1", "nsIProperties");
var OUTDIR = (new DIR_SERVICE()).get("ProfD", Ci.nsIFile).path;
var INDIR = do_get_file("toolkit/components/passwordmgr/test/unit/data/" +
                        "signons-00.txt").parent.path;


function initStorage(aInputPathName,  aInputFileName,
                     aOutputPathName, aOutputFileName, aExpectedError) {
    var e, caughtError = false;

    var inputFile  = Cc["@mozilla.org/file/local;1"]
                            .createInstance(Ci.nsILocalFile);
    inputFile.initWithPath(aInputPathName);
    inputFile.append(aInputFileName);

    var outputFile = null;
    if (aOutputFileName) {
        var outputFile = Cc["@mozilla.org/file/local;1"]
                                .createInstance(Ci.nsILocalFile);
        outputFile.initWithPath(aOutputPathName);
        outputFile.append(aOutputFileName);
    }

    try {
        storage.initWithFile(inputFile, outputFile);
    } catch (e) {
        caughtError = true;
        var err = e;
    }

    if (aExpectedError) {
        if (!caughtError)
            throw "Storage didn't throw as expected (" + aExpectedError + ")";

        if (!aExpectedError.test(err))
            throw "Storage threw (" + err + "), not (" + aExpectedError;

        // We got the expected error, so make a note in the test log.
        dump("...that error was expected.\n\n");
    } else if (caughtError) {
        throw "Component threw unexpected error: " + err;
    }

    return;
};


// Compare info from component to what we expected.
function checkStorageData(ref_disabledHosts, ref_logins) {
    var stor_disabledHosts = storage.getAllDisabledHosts({});
    do_check_eq(ref_disabledHosts.length, stor_disabledHosts.length);
    
    var stor_logins = storage.getAllLogins({});
    do_check_eq(ref_logins.length, stor_logins.length);

    /*
     * Check values of the disabled list. We check both "x in y" and "y in x"
     * to make sure any differences are explicitly noted.
     */
    var i, j, found;
    for (i = 0; i < ref_disabledHosts.length; i++) {
        found = false;
        for (j = 0; !found && j < stor_disabledHosts.length; j++) {
            found = (ref_disabledHosts[i] == stor_disabledHosts[j]);
        }
        do_check_true(found || stor_disabledHosts.length == 0);
    }
    for (j = 0; j < stor_disabledHosts.length; j++) {
        found = false;
        for (i = 0; !found && i < ref_disabledHosts.length; i++) {
            found = (ref_disabledHosts[i] == stor_disabledHosts[j]);
        }
        do_check_true(found || stor_disabledHosts.length == 0);
    }

    /*
     * Check values of the logins list. We check both "x in y" and "y in x"
     * to make sure any differences are explicitly noted.
     */
    for (i = 0; i < ref_logins.length; i++) {
        found = false;
        for (j = 0; !found && j < stor_logins.length; j++) {
            found = ref_logins[i].equals(stor_logins[j]);
        }
        do_check_true(found || stor_logins.length == 0);
    }

}


function run_test() {


try {


/* ========== 0 ========== */
var testnum = 0;
var testdesc = "Initial connection to storage module"

storage = Cc["@mozilla.org/login-manager/storage/legacy;1"]
                    .createInstance(Ci.nsILoginManagerStorage);
if (!storage) throw "Couldn't create storage instance.";


/* ========== 1 ========== */
testnum++;
var testdesc = "Create nsILoginInfo instances for testing with"

var dummyuser1 = Cc["@mozilla.org/login-manager/loginInfo;1"]
                    .createInstance(Ci.nsILoginInfo);
var dummyuser2 = Cc["@mozilla.org/login-manager/loginInfo;1"]
                    .createInstance(Ci.nsILoginInfo);
var dummyuser3 = Cc["@mozilla.org/login-manager/loginInfo;1"]
                    .createInstance(Ci.nsILoginInfo);

dummyuser1.hostname      = "http://dummyhost.mozilla.org";
dummyuser1.username      = "testuser1";
dummyuser1.usernameField = "put_user_here";
dummyuser1.password      = "testpass1";
dummyuser1.passwordField = "put_pw_here";
dummyuser1.formSubmitURL = "";
dummyuser1.httpRealm     = null;

dummyuser2.hostname      = "http://dummyhost2.mozilla.org";
dummyuser2.username      = "testuser2";
dummyuser2.usernameField = "put_user2_here";
dummyuser2.password      = "testpass2";
dummyuser2.passwordField = "put_pw2_here";
dummyuser2.formSubmitURL = "";
dummyuser2.httpRealm     = null;

dummyuser3.hostname      = "http://dummyhost2.mozilla.org";
dummyuser3.username      = "testuser3";
dummyuser3.usernameField = "put_user3_here";
dummyuser3.password      = "testpass3";
dummyuser3.passwordField = "put_pw3_here";
dummyuser3.formSubmitURL = "";
dummyuser3.httpRealm     = null;


var logins, disabledHosts;


/*
 * ---------------------- Bug 380961 ----------------------
 * Need to support decoding the mime64-obscured format still
 * used by SeaMonkey.
 */


/* ========== 2 ========== */
testnum++;

testdesc = "checking import of mime64-obscured entries"
initStorage(INDIR, "signons-380961-1.txt", OUTDIR, "output-380961-1.txt");
checkStorageData([], [dummyuser1]);

testdesc = "[flush and reload for verification]"
initStorage(OUTDIR, "output-380961-1.txt");
checkStorageData([], [dummyuser1]);

/* ========== 3 ========== */
testnum++;

testdesc = "testing import of multiple mime-64 entries for a host"
initStorage(INDIR, "signons-380961-2.txt", OUTDIR, "output-380961-2.txt");
checkStorageData([], [dummyuser2, dummyuser3]);

testdesc = "[flush and reload for verification]"
initStorage(OUTDIR, "output-380961-2.txt");
checkStorageData([], [dummyuser2, dummyuser3]);

/* ========== 4 ========== */
testnum++;

testdesc = "testing import of mixed encrypted and mime-64 entries."
initStorage(INDIR, "signons-380961-3.txt", OUTDIR, "output-380961-3.txt");
checkStorageData([], [dummyuser1, dummyuser2, dummyuser3]);

testdesc = "[flush and reload for verification]"
initStorage(OUTDIR, "output-380961-3.txt");
checkStorageData([], [dummyuser1, dummyuser2, dummyuser3]);


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
var dummyuser4 = Cc["@mozilla.org/login-manager/loginInfo;1"]
                    .createInstance(Ci.nsILoginInfo);

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
initStorage(INDIR, "signons-381262.txt", OUTDIR, "output-381262-1.txt");
var logins = storage.getAllLogins({});
checkStorageData([], [dummyuser4]);

testdesc = "[flush and reload for verification]"
initStorage(OUTDIR, "output-381262-1.txt");
checkStorageData([], [dummyuser4]);


/* ========== 7 ========== */
testnum++;

testdesc = "testing storage of non-ascii username and password."
initStorage(INDIR, "signons-empty.txt", OUTDIR, "output-381262-2.txt");
checkStorageData([], []);
storage.addLogin(dummyuser4);
checkStorageData([], [dummyuser4]);

testdesc = "[flush and reload for verification]"
initStorage(OUTDIR, "output-381262-2.txt");
var logins = storage.getAllLogins({});
checkStorageData([], [dummyuser4]);




} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e); }
};
