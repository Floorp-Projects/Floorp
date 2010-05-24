/*
 * Test suite for storage-mozStorage.js -- exercises reading from on-disk storage.
 *
 * This test interfaces directly with the mozStorage password storage module,
 * bypassing the normal password manager usage.
 *
 */


const STORAGE_TYPE = "mozStorage";

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

var storage;
storage = LoginTest.initStorage(INDIR, "signons-empty.txt", OUTDIR, "signons-empty.sqlite");
storage.getAllLogins();

var testdesc = "[ensuring file exists]"
var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
file.initWithPath(OUTDIR);
file.append("signons-empty.sqlite");
do_check_true(file.exists());

LoginTest.deleteFile(OUTDIR, "signons-empty.sqlite");

/* ========== 2 ========== */
testnum++;
testdesc = "[ensuring file doesn't exist]";

var filename="this-file-does-not-exist.pwmgr.sqlite";
file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
file.initWithPath(OUTDIR);
file.append(filename);
if(file.exists())
    file.remove(false);

testdesc = "Initialize with a nonexistent data file";

storage = LoginTest.reloadStorage(OUTDIR, filename);

LoginTest.checkStorageData(storage, [], []);

try {
    if (file.exists())
        file.remove(false);
} catch (e) { }


/* ========== 3 ========== */
testnum++;
testdesc = "Initialize with signons-02.txt (valid, but empty)";

storage = LoginTest.initStorage(INDIR, "signons-02.txt", OUTDIR, "signons-02.sqlite");
LoginTest.checkStorageData(storage, [], []);

LoginTest.deleteFile(OUTDIR, "signons-02.sqlite");


/* ========== 4 ========== */
testnum++;
testdesc = "Initialize with signons-03.txt (1 disabled, 0 logins)";

storage = LoginTest.initStorage(INDIR, "signons-03.txt", OUTDIR, "signons-03.sqlite");
LoginTest.checkStorageData(storage, ["http://www.disabled.com"], []);

LoginTest.deleteFile(OUTDIR, "signons-03.sqlite");


/* ========== 5 ========== */
testnum++;
testdesc = "Initialize with signons-04.txt (1 disabled, 0 logins, extra '.')";

// Mozilla code should never have generated the extra ".", but it's possible
// someone writing an external utility might have generated it, since it
// would seem consistant with the format.
storage = LoginTest.initStorage(INDIR, "signons-04.txt", OUTDIR, "signons-04.sqlite");
LoginTest.checkStorageData(storage, ["http://www.disabled.com"], []);

LoginTest.deleteFile(OUTDIR, "signons-04.sqlite");


/* ========== 6 ========== */
testnum++;
testdesc = "Initialize with signons-05.txt (0 disabled, 1 login)";

storage = LoginTest.initStorage(INDIR, "signons-05.txt", OUTDIR, "signons-05.sqlite");
LoginTest.checkStorageData(storage, [], [testuser1]);
// counting logins matching host
do_check_eq(1, storage.countLogins("http://dummyhost.mozilla.org", "",    null));
// counting logins matching host (login has blank actionURL)
do_check_eq(1, storage.countLogins("http://dummyhost.mozilla.org", "foo", null));
// counting logins (don't match form login for HTTP search)
do_check_eq(0, storage.countLogins("http://dummyhost.mozilla.org", null,    ""));
// counting logins (don't match a bogus hostname)
do_check_eq(0, storage.countLogins("blah", "", ""));
// counting all logins (empty hostname)
do_check_eq(1, storage.countLogins("", "", null));
// counting all logins (empty hostname)
do_check_eq(1, storage.countLogins("", "foo", null));
// counting no logins (null hostname)
do_check_eq(0, storage.countLogins(null, "", null));
do_check_eq(0, storage.countLogins(null, null, ""));
do_check_eq(0, storage.countLogins(null, "", ""));
do_check_eq(0, storage.countLogins(null, null, null));

LoginTest.deleteFile(OUTDIR, "signons-05.sqlite");


/* ========== 7 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login)";

storage = LoginTest.initStorage(INDIR, "signons-06.txt", OUTDIR, "signons-06.sqlite");
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);

LoginTest.deleteFile(OUTDIR, "signons-06.sqlite");


/* ========== 8 ========== */
testnum++;
testdesc = "Initialize with signons-07.txt (0 disabled, 2 logins on same host)";

storage = LoginTest.initStorage(INDIR, "signons-07.txt", OUTDIR, "signons-07.sqlite");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);
// counting logins matching host
do_check_eq(2, storage.countLogins("http://dummyhost.mozilla.org", "", null));
// counting logins matching host (login has blank actionURL)
do_check_eq(2, storage.countLogins("http://dummyhost.mozilla.org", "foo", null));
// counting logins (don't match form login for HTTP search)
do_check_eq(0, storage.countLogins("http://dummyhost.mozilla.org", null, ""));
// counting logins (don't match a bogus hostname)
do_check_eq(0, storage.countLogins("blah", "", ""));

LoginTest.deleteFile(OUTDIR, "signons-07.sqlite");


/* ========== 9 ========== */
testnum++;
testdesc = "Initialize with signons-08.txt (500 disabled, 500 logins)";

storage = LoginTest.initStorage(INDIR, "signons-08.txt", OUTDIR, "signons-08.sqlite");

var disabledHosts = [];
for (var i = 1; i <= 500; i++) {
    disabledHosts.push("http://host-" + i + ".site.com");
}

var bulkLogin, logins = [];
for (i = 1; i <= 250; i++) {
    bulkLogin = new nsLoginInfo;
    bulkLogin.init("http://dummyhost.site.org", "http://cgi.site.org", null,
        "dummydude", "itsasecret", "usernameField-" + i, "passwordField-" + i);
    logins.push(bulkLogin);
}
for (i = 1; i <= 250; i++) {
    bulkLogin = new nsLoginInfo;
    bulkLogin.init("http://dummyhost-" + i + ".site.org", "http://cgi.site.org", null,
        "dummydude", "itsasecret", "usernameField", "passwordField");
    logins.push(bulkLogin);
}
LoginTest.checkStorageData(storage, disabledHosts, logins);

// counting all logins for dummyhost
do_check_eq(250, storage.countLogins("http://dummyhost.site.org", "", ""));
do_check_eq(250, storage.countLogins("http://dummyhost.site.org", "", null));
do_check_eq(0,   storage.countLogins("http://dummyhost.site.org", null, ""));
// counting all logins for dummyhost-1
do_check_eq(1, storage.countLogins("http://dummyhost-1.site.org", "", ""));
do_check_eq(1, storage.countLogins("http://dummyhost-1.site.org", "", null));
do_check_eq(0, storage.countLogins("http://dummyhost-1.site.org", null, ""));
// counting logins for all hosts
do_check_eq(500, storage.countLogins("", "", ""));
do_check_eq(500, storage.countLogins("", "http://cgi.site.org", ""));
do_check_eq(500, storage.countLogins("", "http://cgi.site.org", null));
do_check_eq(0,   storage.countLogins("", "blah", ""));
do_check_eq(0,   storage.countLogins("", "", "blah"));
// counting logins for no hosts
do_check_eq(0, storage.countLogins(null, "", ""));
do_check_eq(0, storage.countLogins(null, "http://cgi.site.org", ""));
do_check_eq(0, storage.countLogins(null, "http://cgi.site.org", null));
do_check_eq(0, storage.countLogins(null, null, null));

LoginTest.deleteFile(OUTDIR, "signons-08.sqlite");


/* ========== 10 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login); test removeLogin";

storage = LoginTest.initStorage(INDIR, "signons-06.txt", OUTDIR, "signons-06-2.sqlite");
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);

storage.removeLogin(testuser1);
LoginTest.checkStorageData(storage, ["https://www.site.net"], []);

LoginTest.deleteFile(OUTDIR, "signons-06-2.sqlite");


/* ========== 11 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login); test modifyLogin";

storage = LoginTest.initStorage(INDIR, "signons-06.txt", OUTDIR, "signons-06-3.sqlite");
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);

// Try modifying a nonexistent login
var err = null;
try {
    storage.modifyLogin(testuser2, testuser1);
} catch (e) {
    err = e;
}
LoginTest.checkExpectedError(/No matching logins/, err);
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);

// Really modify the login.
storage.modifyLogin(testuser1, testuser2);
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser2]);

LoginTest.deleteFile(OUTDIR, "signons-06-3.sqlite");


/*
 * ---------------------- Bug 427033 ----------------------
 * Check migration of logins stored with a JS formSubmitURL
 */


/* ========== 12 ========== */
testnum++;

testdesc = "checking import of JS formSubmitURL entries"

testuser1.init("http://jstest.site.org", "javascript:", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");
storage = LoginTest.initStorage(INDIR, "signons-427033-1.txt",
                               OUTDIR, "signons-427033-1.sqlite");
LoginTest.checkStorageData(storage, [], [testuser1]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "signons-427033-1.sqlite");
LoginTest.checkStorageData(storage, [], [testuser1]);

LoginTest.deleteFile(OUTDIR, "signons-427033-1.sqlite");


/*
 * ---------------------- Bug 500822 ----------------------
 * Importing passwords to mozstorage can fail when signons3.txt is corrupted.
 */


/* ========== 13 ========== */
testnum++;

testdesc = "checking import of partially corrupted signons3.txt"

testuser1.init("http://dummyhost.mozilla.org", "", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");
storage = LoginTest.initStorage(INDIR, "signons-500822-1.txt",
                               OUTDIR, "signons-500822-1.sqlite");

// Using searchLogins to check that we have the correct first entry. Tests for
// searchLogin are in test_storage_mozStorage_7.js. Other entries may be
// corrupted, but we need to check for one valid login.
let matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("hostname", "http://dummyhost.mozilla.org");
matchData.setPropertyAsAString("usernameField", "put_user_here");
logins = storage.searchLogins({}, matchData);
do_check_eq(1, logins.length, "should match 1 login");

LoginTest.deleteFile(OUTDIR, "signons-500822-1.sqlite");


} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
};
