/*
 * Test suite for storage-Legacy.js -- exercises reading from on-disk storage.
 *
 * This test interfaces directly with the legacy password storage module,
 * bypassing the normal password manager usage.
 *
 */

var storage;

var DIR_SERVICE = new Components.Constructor(
                    "@mozilla.org/file/directory_service;1", "nsIProperties");
var OUTDIR = (new DIR_SERVICE()).get("ProfD", Ci.nsIFile).path;
var INDIR = do_get_file("toolkit/components/passwordmgr/test/unit/data/" +
                        "signons-00.txt").parent.path;


function initStorage(aInputPathName, aInputFileName, aExpectedError) {
    var e, caughtError = false;

    var inputFile  = Cc["@mozilla.org/file/local;1"]
                            .createInstance(Ci.nsILocalFile);
    inputFile.initWithPath(aInputPathName);
    inputFile.append(aInputFileName);

    try {
        storage.initWithFile(inputFile, null);
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
    for (i = 0; i < ref_logins.length; i++) {
        for (j = 0; !found && j < stor_logins.length; j++) {
            found = ref_logins[i].equals(stor_logins[j]);
        }
        do_check_true(found || stor_logins.length == 0);
    }

}



function run_test() {
try {

var testnum = 0;
var testdesc = "Setup of nsLoginInfo test-users";
var nsLoginInfo = new Components.Constructor(
                    "@mozilla.org/login-manager/loginInfo;1",
                    Components.interfaces.nsILoginInfo);
do_check_true(nsLoginInfo != null);

var testuser1 = new nsLoginInfo;
testuser1.hostname      = "http://dummyhost.mozilla.org";
testuser1.formSubmitURL = "";
testuser1.username      = "dummydude";
testuser1.password      = "itsasecret";
testuser1.usernameField = "put_user_here";
testuser1.passwordField = "put_pw_here";
testuser1.httpRealm     = null;

var testuser2 = new nsLoginInfo;
testuser2.hostname      = "http://dummyhost.mozilla.org";
testuser2.formSubmitURL = "";
testuser2.username      = "dummydude2";
testuser2.password      = "itsasecret2";
testuser2.usernameField = "put_user2_here";
testuser2.passwordField = "put_pw2_here";
testuser2.httpRealm     = null;

var logins, disabledHosts;

dump("/* ========== 1 ========== */\n");
var testnum = 1;
var testdesc = "Initial connection to storage module"

storage = Cc["@mozilla.org/login-manager/storage/legacy;1"]
                .createInstance(Ci.nsILoginManagerStorage);
if (!storage) throw "Couldn't create storage instance.";


dump("/* ========== 2 ========== */\n");
testnum++;
testdesc = "[ensuring file doesn't exist]";

var filename="this-file-does-not-exist-"+Math.floor(Math.random() * 10000);
var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
file.initWithPath(OUTDIR);
file.append(filename);
var exists = file.exists();
if (exists) {
    // Go ahead and remove the file, so that this failure doesn't
    // repeat itself w/o intervention.
    file.remove(false);
    do_check_false(exists);
}

testdesc = "Initialize with a non-existant data file";

initStorage(OUTDIR, filename, null);

checkStorageData([], []);

file.remove(false);

dump("/* ========== 3 ========== */\n");
testnum++;
testdesc = "Initialize with signons-00.txt (a zero-length file)";

initStorage(INDIR, "signons-00.txt", /invalid file header/);
checkStorageData([], []);


/* ========== 4 ========== */
testnum++;
testdesc = "Initialize with signons-01.txt (bad file header)";

initStorage(INDIR, "signons-01.txt", /invalid file header/);
checkStorageData([], []);


/* ========== 5 ========== */
testnum++;
testdesc = "Initialize with signons-02.txt (valid, but empty)";

initStorage(INDIR, "signons-02.txt", null);
checkStorageData([], []);


/* ========== 6 ========== */
testnum++;
testdesc = "Initialize with signons-03.txt (1 disabled, 0 logins)";

initStorage(INDIR, "signons-03.txt", null);

disabledHosts = ["http://www.disabled.com"];
checkStorageData(disabledHosts, []);


/* ========== 7 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 0 logins, extra '.')";

// Mozilla code should never have generated the extra ".", but it's possible
// someone writing an external utility might have generated it, since it
// would seem consistant with the format.
initStorage(INDIR, "signons-04.txt", null);

disabledHosts = ["http://www.disabled.com"];
checkStorageData(disabledHosts, []);


/* ========== 8 ========== */
testnum++;
testdesc = "Initialize with signons-05.txt (0 disabled, 1 login)";

initStorage(INDIR, "signons-05.txt", null);

logins = [testuser1];
checkStorageData([], logins);


/* ========== 9 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login)";

initStorage(INDIR, "signons-06.txt", null);

disabledHosts = ["https://www.site.net"];
logins = [testuser1];
checkStorageData(disabledHosts, logins);


/* ========== 10 ========== */
testnum++;
testdesc = "Initialize with signons-07.txt (0 disabled, 2 logins for same host)";

initStorage(INDIR, "signons-07.txt", null);

logins = [testuser1, testuser2];
checkStorageData([], logins);


/* ========== 11 ========== */
testnum++;
testdesc = "Initialize with signons-08.txt (1000 disabled, 1000 logins)";

initStorage(INDIR, "signons-08.txt", null);

disabledHosts = [];
for (var i = 1; i <= 1000; i++) {
    disabledHosts.push("http://host-" + i + ".site.com");
}

logins = [];
for (i = 1; i <= 500; i++) {
    logins.push("http://dummyhost.site.org");
}
for (i = 1; i <= 500; i++) {
    logins.push("http://dummyhost-" + i + ".site.org");
}
checkStorageData(disabledHosts, logins);


} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e; }
};
