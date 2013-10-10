/*
 * Test suite for storage-mozStorage.js -- Testing searchLogins.
 *
 * This test interfaces directly with the mozStorage login storage module,
 * bypassing the normal login manager usage.
 *
 */


function run_test() {

try {

var storage, testnum = 0;


/* ========== 1 ========== */
testnum++;
var testdesc = "Create nsILoginInfo instances for testing with"

var dummyuser1 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser2 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser3 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser4 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);

dummyuser1.init("http://dummyhost.mozilla.org", "", null,
    "testuser1", "testpass1", "put_user_here", "put_pw_here");

dummyuser2.init("http://dummyhost2.mozilla.org", "", null,
    "testuser2", "testpass2", "put_user2_here", "put_pw2_here");

dummyuser3.init("http://dummyhost2.mozilla.org", "http://dummyhost2.mozilla.org", null,
    "testuser3", "testpass3", "put_user3_here", "put_pw3_here");
    
dummyuser4.init("http://dummyhost3.mozilla.org", null, null,
    "testuser4", "testpass4", "put_user4_here", "put_pw4_here");


/* ========== 2 ========== */
testnum++;

testdesc = "checking that searchLogins works with values passed"
storage = LoginTest.initStorage(OUTDIR, "output-searchLogins-1.sqlite");

storage.addLogin(dummyuser1);
storage.addLogin(dummyuser2);
storage.addLogin(dummyuser3);

LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3]);

let matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("id", "1");
let logins = storage.searchLogins({}, matchData);
do_check_eq(1, logins.length, "expecting single login with id");

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
logins = storage.searchLogins({}, matchData);
do_check_eq(3, logins.length, "should match all logins");

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("hostname", "http://dummyhost2.mozilla.org");
logins = storage.searchLogins({}, matchData);
do_check_eq(2, logins.length, "should match some logins");

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("hostname", "http://dummyhost2.mozilla.org");
matchData.setPropertyAsAString("httpRealm", null);
logins = storage.searchLogins({}, matchData);
do_check_eq(2, logins.length, "should match some logins");

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("formSubmitURL", "");
logins = storage.searchLogins({}, matchData);
do_check_eq(2, logins.length, "should match some logins");


/* ========== 3 ========== */
testnum++;

// uses storage from test #2
testdesc = "checking that searchLogins throws when it's supposed"

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("id", "100");
logins = storage.searchLogins({}, matchData);
do_check_eq(0, logins.length, "bogus value should return 0 results");

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("error", "value");
let error;
try {
    logins = storage.searchLogins({}, matchData);
} catch (e) {
    error = e;
}
LoginTest.checkExpectedError(/Unexpected field/, error, "nonexistent field should throw");

LoginTest.deleteFile(OUTDIR, "output-searchLogins-1.sqlite");


/* ========== 4 ========== */
testnum++;

testdesc = "checking that searchLogins works as findLogins"
storage = LoginTest.initStorage(OUTDIR, "output-searchLogins-3.sqlite");

storage.addLogin(dummyuser1);
storage.addLogin(dummyuser2);
storage.addLogin(dummyuser3);
storage.addLogin(dummyuser4);

LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3, dummyuser4]);

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
logins = storage.searchLogins({}, matchData);
let loginsF = storage.findLogins({}, "", "", "");
LoginTest.checkLogins(loginsF, logins);

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("hostname", "http://dummyhost2.mozilla.org");
logins = storage.searchLogins({}, matchData);
loginsF = storage.findLogins({}, "http://dummyhost2.mozilla.org", "", "");
LoginTest.checkLogins(loginsF, logins);

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("hostname", "http://dummyhost2.mozilla.org");
matchData.setPropertyAsAString("formSubmitURL", "http://dummyhost2.mozilla.org");
logins = storage.searchLogins({}, matchData);
loginsF = storage.findLogins({}, "http://dummyhost2.mozilla.org", "http://dummyhost2.mozilla.org", "");
LoginTest.checkLogins(loginsF, logins);

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("formSubmitURL", "http://dummyhost2.mozilla.org");
matchData.setPropertyAsAString("httpRealm", null);
logins = storage.searchLogins({}, matchData);
loginsF = storage.findLogins({}, "", "http://dummyhost2.mozilla.org", null);
LoginTest.checkLogins(loginsF, logins);

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
matchData.setPropertyAsAString("formSubmitURL", null);
matchData.setPropertyAsAString("httpRealm", null);
logins = storage.searchLogins({}, matchData);
loginsF = storage.findLogins({}, "", null, null);
LoginTest.checkLogins(loginsF, logins);

LoginTest.deleteFile(OUTDIR, "output-searchLogins-3.sqlite");


/* ========== end ========== */
} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

};
