/*
 * Test suite for storage-Legacy.js -- exercises reading from on-disk storage.
 *
 * This test interfaces directly with the legacy password storage module,
 * bypassing the normal password manager usage.
 *
 */


function run_test() {

try {

var testnum = 0;
var testdesc = "Setup of nsLoginInfo test-users";
var nsLoginInfo = new Components.Constructor(
                    "@mozilla.org/login-manager/loginInfo;1",
                    Components.interfaces.nsILoginInfo);
do_check_true(nsLoginInfo != null);

var testuser1 = new nsLoginInfo;
testuser1.init("http://dummyhost.mozilla.org", "", null,
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");

var testuser2 = new nsLoginInfo;
testuser2.init("http://dummyhost.mozilla.org", "", null,
    "dummydude2", "itsasecret2", "put_user2_here", "put_pw2_here");


/* ========== 1 ========== */
var testnum = 1;
var testdesc = "Initial connection to storage module"

var storage = Cc["@mozilla.org/login-manager/storage/legacy;1"].
              createInstance(Ci.nsILoginManagerStorage);
if (!storage)
    throw "Couldn't create storage instance.";


/* ========== 2 ========== */
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
    do_check_false(exists); // fail on purpose
}

testdesc = "Initialize with a non-existant data file";

LoginTest.initStorage(storage, OUTDIR, filename);

LoginTest.checkStorageData(storage, [], []);

file.remove(false);

/* ========== 3 ========== */
testnum++;
testdesc = "Initialize with signons-00.txt (a zero-length file)";

LoginTest.initStorage(storage, INDIR, "signons-00.txt",
                     null, null, /invalid file header/);
LoginTest.checkStorageData(storage, [], []);


/* ========== 4 ========== */
testnum++;
testdesc = "Initialize with signons-01.txt (bad file header)";

LoginTest.initStorage(storage, INDIR, "signons-01.txt",
                     null, null, /invalid file header/);
LoginTest.checkStorageData(storage, [], []);


/* ========== 5 ========== */
testnum++;
testdesc = "Initialize with signons-02.txt (valid, but empty)";

LoginTest.initStorage(storage, INDIR, "signons-02.txt");
LoginTest.checkStorageData(storage, [], []);


/* ========== 6 ========== */
testnum++;
testdesc = "Initialize with signons-03.txt (1 disabled, 0 logins)";

LoginTest.initStorage(storage, INDIR, "signons-03.txt");
LoginTest.checkStorageData(storage, ["http://www.disabled.com"], []);


/* ========== 7 ========== */
testnum++;
testdesc = "Initialize with signons-04.txt (1 disabled, 0 logins, extra '.')";

// Mozilla code should never have generated the extra ".", but it's possible
// someone writing an external utility might have generated it, since it
// would seem consistant with the format.
LoginTest.initStorage(storage, INDIR, "signons-04.txt");
LoginTest.checkStorageData(storage, ["http://www.disabled.com"], []);


/* ========== 8 ========== */
testnum++;
testdesc = "Initialize with signons-05.txt (0 disabled, 1 login)";

LoginTest.initStorage(storage, INDIR, "signons-05.txt");
LoginTest.checkStorageData(storage, [], [testuser1]);
// counting logins matching host
do_check_eq(1, storage.countLogins("http://dummyhost.mozilla.org", "",    null));
// counting logins matching host (login has blank actionURL)
do_check_eq(1, storage.countLogins("http://dummyhost.mozilla.org", "foo", null));
// counting logins (don't match form login for HTTP search)
do_check_eq(0, storage.countLogins("http://dummyhost.mozilla.org", null,    ""));
// counting logins (don't match a bogus hostname)
do_check_eq(0, storage.countLogins("blah", "", ""));


/* ========== 9 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login)";

LoginTest.initStorage(storage, INDIR, "signons-06.txt");
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);


/* ========== 10 ========== */
testnum++;
testdesc = "Initialize with signons-07.txt (0 disabled, 2 logins on same host)";

LoginTest.initStorage(storage, INDIR, "signons-07.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);
// counting logins matching host
do_check_eq(2, storage.countLogins("http://dummyhost.mozilla.org", "", null));
// counting logins matching host (login has blank actionURL)
do_check_eq(2, storage.countLogins("http://dummyhost.mozilla.org", "foo", null));
// counting logins (don't match form login for HTTP search)
do_check_eq(0, storage.countLogins("http://dummyhost.mozilla.org", null, ""));
// counting logins (don't match a bogus hostname)
do_check_eq(0, storage.countLogins("blah", "", ""));


/* ========== 11 ========== */
testnum++;
testdesc = "Initialize with signons-08.txt (1000 disabled, 1000 logins)";

LoginTest.initStorage(storage, INDIR, "signons-08.txt");

var disabledHosts = [];
for (var i = 1; i <= 1000; i++) {
    disabledHosts.push("http://host-" + i + ".site.com");
}

var logins = [];
for (i = 1; i <= 500; i++) {
    logins.push("http://dummyhost.site.org");
}
for (i = 1; i <= 500; i++) {
    logins.push("http://dummyhost-" + i + ".site.org");
}
LoginTest.checkStorageData(storage, disabledHosts, logins);

// counting all logins for dummyhost
do_check_eq(500, storage.countLogins("http://dummyhost.site.org", "", ""));
// counting all logins for dummyhost-1
do_check_eq(1, storage.countLogins("http://dummyhost-1.site.org", "", ""));

} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
};
