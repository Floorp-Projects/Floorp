/*
 * Test suite for storage-Legacy.js -- exercises writing to on-disk storage.
 *
 * This test interfaces directly with the legacy login storage module,
 * bypassing the normal login manager usage.
 *
 */

var storage;

var DIR_SERVICE = new Components.Constructor(
                    "@mozilla.org/file/directory_service;1", "nsIProperties");
var OUTDIR = (new DIR_SERVICE()).get("ProfD", Ci.nsIFile).path + "/";
var INDIR = do_get_file("toolkit/components/passwordmgr/test/unit/data/" +
                        "signons-00.txt").parent.path + "/";


function initStorage(aInputFileName, aOutputFileName, aExpectedError) {
    var e, caughtError = false;

    var inputFile  = Cc["@mozilla.org/file/local;1"]
                            .createInstance(Ci.nsILocalFile);
    inputFile.initWithPath(aInputFileName);

    var outputFile = null;
    if (aOutputFileName) {
        var outputFile = Cc["@mozilla.org/file/local;1"]
                                .createInstance(Ci.nsILocalFile);
        outputFile.initWithPath(aOutputFileName);
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
        for (j = 0; !found && j < stor_disabledHosts.length; j++) {
            found = (ref_disabledHosts[i] == stor_disabledHosts[j]);
        }
        do_check_true(found || stor_disabledHosts.length == 0);
    }
    for (j = 0; j < stor_disabledHosts.length; j++) {
        for (i = 0; !found && i < ref_disabledHosts.length; i++) {
            found = (ref_disabledHosts[i] == stor_disabledHosts[j]);
        }
        do_check_true(found || stor_disabledHosts.length == 0);
    }

    /*
     * Check values of the logins list. We check both "x in y" and "y in x"
     * to make sure any differences are explicitly noted.
     */
    var ref, stor;
    for (i = 0; i < ref_logins.length; i++) {
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
dummyuser1.username      = "dummydude";
dummyuser1.usernameField = "put_user_here";
dummyuser1.password      = "itsasecret";
dummyuser1.passwordField = "put_pw_here";
dummyuser1.formSubmitURL = "";
dummyuser1.httpRealm     = null;

dummyuser2.hostname      = "http://dummyhost2.mozilla.org";
dummyuser2.username      = "dummydude2";
dummyuser2.usernameField = "put_user2_here";
dummyuser2.password      = "itsasecret2";
dummyuser2.passwordField = "put_pw2_here";
dummyuser2.formSubmitURL = "http://cgi.site.com";
dummyuser2.httpRealm     = null;

dummyuser3.hostname      = "http://dummyhost2.mozilla.org";
dummyuser3.username      = "dummydude3";
dummyuser3.usernameField = "put_user3_here";
dummyuser3.password      = "itsasecret3";
dummyuser3.passwordField = "put_pw3_here";
dummyuser3.formSubmitURL = "http://cgi.site.com";
dummyuser3.httpRealm     = null;

var logins, disabledHosts;

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
    do_check_false(exists);
}

testdesc = "Initialize with no existing file";
initStorage(OUTDIR + filename, null);

checkStorageData([], []);

testdesc = "Add 1 disabled host only";
storage.setLoginSavingEnabled("http://www.nsa.gov", false);
disabledHosts = ["http://www.nsa.gov"];
checkStorageData(disabledHosts, []);

file.remove(false);



/* ========== 3 ========== */
testnum++;

testdesc = "Initialize with existing file (valid, but empty)";
initStorage(INDIR + "signons-empty.txt", OUTDIR + "output-01.txt");
checkStorageData([], []);

testdesc = "Add 1 disabled host only";
storage.setLoginSavingEnabled("http://www.nsa.gov", false);

disabledHosts = ["http://www.nsa.gov"];
checkStorageData(disabledHosts, []);

testdesc = "Remove disabled host only";
storage.setLoginSavingEnabled("http://www.nsa.gov", true);

checkStorageData([], []);


/* ========== 4 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
initStorage(INDIR + "signons-empty.txt", OUTDIR + "output-02.txt");

// Previous tests made sure we can write to an existing file, so now just
// tweak component to output elsewhere.
testdesc = "Add 1 login only";
storage.addLogin(dummyuser1);

testdesc = "[flush and reload for verification]";
initStorage(OUTDIR + "output-02.txt", null);

testdesc = "Verify output-02.txt";
checkStorageData([], [dummyuser1]);


/* ========== 5 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
initStorage(INDIR + "signons-empty.txt", OUTDIR + "output-03.txt");

testdesc = "Add 1 disabled host only";
storage.setLoginSavingEnabled("http://www.nsa.gov", false);

testdesc = "[flush and reload for verification]";
initStorage(OUTDIR + "output-03.txt", null);

testdesc = "Verify output-03.txt";
checkStorageData(["http://www.nsa.gov"], []);


/* ========== 6 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
initStorage(INDIR + "signons-empty.txt", OUTDIR + "output-04.txt");

testdesc = "Add 1 disabled host and 1 login";
storage.setLoginSavingEnabled("http://www.nsa.gov", false);
storage.addLogin(dummyuser1);

testdesc = "[flush and reload for verification]";
initStorage(OUTDIR + "output-04.txt", null);

testdesc = "Verify output-04.txt";
checkStorageData(["http://www.nsa.gov"], [dummyuser1]);


/* ========== 7 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
initStorage(INDIR + "signons-empty.txt", OUTDIR + "output-03.txt");

testdesc = "Add 2 logins (to different hosts)";
storage.addLogin(dummyuser1);
storage.addLogin(dummyuser2);

testdesc = "[flush and reload for verification]";
initStorage(OUTDIR + "output-03.txt", null);

testdesc = "Verify output-03.txt";
checkStorageData([], [dummyuser2, dummyuser1]);


/* ========== 8 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
initStorage(INDIR + "signons-empty.txt", OUTDIR + "output-04.txt");

testdesc = "Add 2 logins (to same host)";
storage.addLogin(dummyuser2);
storage.addLogin(dummyuser3);

testdesc = "[flush and reload for verification]";
initStorage(OUTDIR + "output-04.txt", null);

testdesc = "Verify output-04.txt";
logins = [dummyuser3, dummyuser2];
checkStorageData([], logins);


/* ========== 9 ========== */
testnum++;

testdesc = "[clear data and reinitialize with signons-empty.txt]";
initStorage(INDIR + "signons-empty.txt", OUTDIR + "output-05.txt");

testdesc = "Add 3 logins (2 to same host)";
storage.addLogin(dummyuser3);
storage.addLogin(dummyuser1);
storage.addLogin(dummyuser2);

testdesc = "[flush and reload for verification]";
initStorage(OUTDIR + "output-05.txt", null);

testdesc = "Verify output-05.txt";
logins = [dummyuser1, dummyuser2, dummyuser3];
checkStorageData([], logins);

/* ========== 10 ========== */
testnum++;

// test writing a file that already contained entries
// test overwriting a file early
// test permissions on output file (osx/unix)
// test starting with an invalid file, and a non-existant file.
// excercise case where we immediately write the data on init, for empty entries
// test writing a gazillion entries

} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e); }
};
