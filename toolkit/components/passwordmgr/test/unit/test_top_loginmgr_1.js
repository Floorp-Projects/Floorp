/*
 * Unit tests for nsLoginManager.js
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


/* ========== 1 ========== */
var testnum = 1;
var testdesc = "Initial connection to login manager"

var loginmgr = Cc["@mozilla.org/login-manager;1"].
               createInstance(Ci.nsILoginManager);
if (!loginmgr)
    throw "Couldn't create loginmgr instance.";


/* ========== 2 ========== */
testnum++;
testdesc = "Force lazy init, check to ensure there is no existing data.";
loginmgr.removeAllLogins();
var hosts = loginmgr.getAllDisabledHosts();
hosts.forEach(function(h) loginmgr.setLoginSavingEnabled(h, true));
LoginTest.checkStorageData(loginmgr, [], []);


/* ========== 3 ========== */
testnum++;
testdesc = "Try adding invalid logins (host / user / pass checks)";

function tryAddUser(module, aUser, aExpectedError) {
    var err = null;
    try {
        module.addLogin(aUser);
    } catch (e) {
        err = e; 
    }            

    LoginTest.checkExpectedError(aExpectedError, err);
}

testuser1.init(null, null, "Some Realm",
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");
// null hostname
tryAddUser(loginmgr, testuser1, /null or empty hostname/);
LoginTest.checkStorageData(loginmgr, [], []);

testuser1.hostname = "";
tryAddUser(loginmgr, testuser1, /null or empty hostname/);
LoginTest.checkStorageData(loginmgr, [], []);


testuser1.hostname = "http://dummyhost.mozilla.org";
testuser1.username = null;
tryAddUser(loginmgr, testuser1, /null username/);
LoginTest.checkStorageData(loginmgr, [], []);

testuser1.username = "dummydude";
testuser1.password = null;
tryAddUser(loginmgr, testuser1, /null or empty password/);
LoginTest.checkStorageData(loginmgr, [], []);

testuser1.password = "";
tryAddUser(loginmgr, testuser1, /null or empty password/);
LoginTest.checkStorageData(loginmgr, [], []);


/* ========== 4 ========== */
testnum++;
testdesc = "Try adding invalid logins (httpRealm / formSubmitURL checks)";

testuser1.init("http://dummyhost.mozilla.org", null, null,
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");
// formSubmitURL == null,  httpRealm == null
tryAddUser(loginmgr, testuser1, /without a httpRealm or formSubmitURL/);
LoginTest.checkStorageData(loginmgr, [], []);

testuser1.formSubmitURL = "";
testuser1.httpRealm = "";
tryAddUser(loginmgr, testuser1, /both a httpRealm and formSubmitURL/);
LoginTest.checkStorageData(loginmgr, [], []);

testuser1.formSubmitURL = "foo";
testuser1.httpRealm = "foo";
tryAddUser(loginmgr, testuser1, /both a httpRealm and formSubmitURL/);
LoginTest.checkStorageData(loginmgr, [], []);


testuser1.formSubmitURL = "";
testuser1.httpRealm = "foo";
tryAddUser(loginmgr, testuser1, /both a httpRealm and formSubmitURL/);
LoginTest.checkStorageData(loginmgr, [], []);

testuser1.formSubmitURL = "foo";
testuser1.httpRealm = "";
tryAddUser(loginmgr, testuser1, /both a httpRealm and formSubmitURL/);
LoginTest.checkStorageData(loginmgr, [], []);


testuser1.formSubmitURL = null;
testuser1.httpRealm = "";
tryAddUser(loginmgr, testuser1, /without a httpRealm or formSubmitURL/);
LoginTest.checkStorageData(loginmgr, [], []);


/* ========== 5 ========== */
testnum++;
testdesc = "Try adding valid logins (httpRealm / formSubmitURL checks)";

testuser1.formSubmitURL = null;
testuser1.httpRealm = "foo";
testuser1.hostname ="http://dummyhost1";
tryAddUser(loginmgr, testuser1);

testuser1.formSubmitURL = "";
testuser1.httpRealm = null;
testuser1.hostname ="http://dummyhost2";
tryAddUser(loginmgr, testuser1);

testuser1.formSubmitURL = "foo";
testuser1.httpRealm = null;
testuser1.hostname ="http://dummyhost3";
tryAddUser(loginmgr, testuser1);


} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
};
