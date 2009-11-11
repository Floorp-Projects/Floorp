/*
 * Test suite for storage-Legacy.js -- IE Migration Helper.
 *
 * This test exercises the migrateAndAddLogin helper interface.
 * Although in the real-world it would only be invoked on Windows
 * platforms, there's no point in limiting what platforms run
 * this test.
 *
 */


const STORAGE_TYPE = "legacy";

function cloneLogin(src, dst) {
    dst.hostname      = src.hostname;
    dst.formSubmitURL = src.formSubmitURL;
    dst.httpRealm     = src.httpRealm;
    dst.username      = src.username;
    dst.password      = src.password;
    dst.usernameField = src.usernameField;
    dst.passwordField = src.passwordField;
}


function run_test() {

try {


/* ========== 0 ========== */
var testnum = 0;
var testdesc = "Initial connection to storage module"

var storage = Cc["@mozilla.org/login-manager/storage/legacy;1"].
              getService(Ci.nsILoginManagerIEMigrationHelper);
if (!storage)
    throw "Couldn't create storage instance.";

var pwmgr = Cc["@mozilla.org/login-manager;1"].
            getService(Ci.nsILoginManager);
if (!pwmgr)
    throw "Couldn't create pwmgr instance.";

// Start with a clean slate
pwmgr.removeAllLogins();
var hosts = pwmgr.getAllDisabledHosts();
hosts.forEach(function(h) pwmgr.setLoginSavingEnabled(h, true));


/* ========== 1 ========== */
testnum++;
var testdesc = "Create nsILoginInfo instances for testing with"

var testlogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                createInstance(Ci.nsILoginInfo);
var reflogin  = Cc["@mozilla.org/login-manager/loginInfo;1"].
                createInstance(Ci.nsILoginInfo);
var reflogin2 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                createInstance(Ci.nsILoginInfo);


/* ========== 2 ========== */
testnum++;

testdesc = "http auth, port 80";
testlogin.init("example.com:80", null, "Port 80",
               "username", "password", "", "");
cloneLogin(testlogin, reflogin);
reflogin.hostname = "http://example.com";

storage.migrateAndAddLogin(testlogin);
do_check_eq(testlogin.hostname, "http://example.com");

LoginTest.checkStorageData(pwmgr, [], [reflogin]);
pwmgr.removeAllLogins();

/* ========== 3 ========== */
testnum++;

testdesc = "http auth, port 443";
testlogin.init("example.com:443", null, "Port 443",
               "username", "password", "", "");
cloneLogin(testlogin, reflogin);
reflogin.hostname = "https://example.com";

storage.migrateAndAddLogin(testlogin);
do_check_eq(testlogin.hostname, "https://example.com");

LoginTest.checkStorageData(pwmgr, [], [reflogin]);
pwmgr.removeAllLogins();

/* ========== 4 ========== */
testnum++;

testdesc = "http auth, port 4242";
testlogin.init("example.com:4242", null, "Port 4242",
               "username", "password", "", "");
cloneLogin(testlogin, reflogin);
cloneLogin(testlogin, reflogin2);
reflogin.hostname  = "http://example.com:4242";
reflogin2.hostname = "https://example.com:4242";
storage.migrateAndAddLogin(testlogin);

// scheme is ambigious, so 2 logins are created.
LoginTest.checkStorageData(pwmgr, [], [reflogin, reflogin2]);
pwmgr.removeAllLogins();

/* ========== 5 ========== */
testnum++;

testdesc = "http auth, port 80, no realm";
testlogin.init("example.com:80", null, "",
               "username", "password", "", "");
cloneLogin(testlogin, reflogin);
reflogin.hostname  = "http://example.com";
reflogin.httpRealm = "http://example.com";

storage.migrateAndAddLogin(testlogin);
do_check_eq(testlogin.httpRealm, "http://example.com");
LoginTest.checkStorageData(pwmgr, [], [reflogin]);
pwmgr.removeAllLogins();

/* ========== 6 ========== */
testnum++;

testdesc = "form auth, http";
testlogin.init("http://example.com", "", null,
               "username", "password", "uname", "");
cloneLogin(testlogin, reflogin);
// nothing changes, so no need to edit reflogin

storage.migrateAndAddLogin(testlogin);
LoginTest.checkStorageData(pwmgr, [], [reflogin]);
pwmgr.removeAllLogins();

/* ========== 7 ========== */
testnum++;

testdesc = "form auth, https";
testlogin.init("https://example.com", "", null,
               "username", "password", "uname", "");
cloneLogin(testlogin, reflogin);
// nothing changes, so no need to edit reflogin

storage.migrateAndAddLogin(testlogin);
LoginTest.checkStorageData(pwmgr, [], [reflogin]);
pwmgr.removeAllLogins();


/* ========== end ========== */
} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

};
