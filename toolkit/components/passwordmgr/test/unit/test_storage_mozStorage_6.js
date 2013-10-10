/*
 * Test suite for storage-mozStorage.js
 *
 * This test interfaces directly with the mozStorage password storage module,
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
testuser1.QueryInterface(Ci.nsILoginMetaInfo);
testuser1.init("http://testhost1", "", null,
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");
var guid1;

var testuser2 = new nsLoginInfo;
testuser2.init("http://testhost2", "", null,
    "dummydude2", "itsasecret2", "put_user2_here", "put_pw2_here");
testuser2.QueryInterface(Ci.nsILoginMetaInfo);
var guid2 = "{12345678-abcd-1234-abcd-987654321000}";
testuser2.guid = guid2;

var testuser3 = new nsLoginInfo;
testuser3.QueryInterface(Ci.nsILoginMetaInfo);
testuser3.init("http://testhost3", "", null,
    "dummydude3", "itsasecret3", "put_user3_here", "put_pw3_here");
var guid3 = "{99999999-abcd-9999-abcd-999999999999}";

// This login is different than testuser2, except it has the same guid.
var testuser2dupeguid = new nsLoginInfo;
testuser2dupeguid.QueryInterface(Ci.nsILoginMetaInfo);
testuser2dupeguid.init("http://dupe-testhost2", "", null,
    "dupe-dummydude2", "dupe-itsasecret2", "put_user2_here", "put_pw2_here");
testuser2dupeguid.QueryInterface(Ci.nsILoginMetaInfo);
testuser2dupeguid.guid = guid2;

var isGUID = /^\{[0-9a-f\d]{8}-[0-9a-f\d]{4}-[0-9a-f\d]{4}-[0-9a-f\d]{4}-[0-9a-f\d]{12}\}$/;

/* ========== 1 ========== */
var testnum = 1;
var testdesc = "Initial connection to storage module"

LoginTest.deleteFile(OUTDIR, "signons-unittest6.sqlite");

var storage;
storage = LoginTest.initStorage(OUTDIR, "signons-unittest6.sqlite");
var logins = storage.getAllLogins();
do_check_eq(logins.length, 0, "Checking for no initial logins");
var disabledHosts = storage.getAllDisabledHosts();
do_check_eq(disabledHosts.length, 0, "Checking for no initial disabled hosts");


/* ========== 2 ========== */
testnum++;
testdesc = "add user1 w/o guid";

storage.addLogin(testuser1);
LoginTest.checkStorageData(storage, [], [testuser1]);

// Check guid
do_check_eq(testuser1.guid, null, "caller's login shouldn't be modified");
logins = storage.findLogins({}, "http://testhost1", "", null);
do_check_eq(logins.length, 1, "expecting 1 login");
logins[0].QueryInterface(Ci.nsILoginMetaInfo);
do_check_true(isGUID.test(logins[0].guid), "testuser1 guid is set");
guid1 = logins[0].guid;

/* ========== 3 ========== */
testnum++;
testdesc = "add user2 WITH guid";

storage.addLogin(testuser2);
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);

// Check guid
do_check_eq(testuser2.guid, guid2, "caller's login shouldn't be modified");
logins = storage.findLogins({}, "http://testhost2", "", null);
do_check_eq(logins.length, 1, "expecting 1 login");
logins[0].QueryInterface(Ci.nsILoginMetaInfo);
do_check_true(isGUID.test(logins[0].guid), "testuser2 guid is set");
do_check_eq(logins[0].guid, guid2, "checking guid2");

/* ========== 4 ========== */
testnum++;
testdesc = "add user3 w/o guid";

storage.addLogin(testuser3);
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3]);
logins = storage.findLogins({}, "http://testhost3", "", null);
do_check_eq(logins.length, 1, "expecting 1 login");
logins[0].QueryInterface(Ci.nsILoginMetaInfo);
do_check_true(isGUID.test(logins[0].guid), "testuser3 guid is set");
do_check_neq(logins[0].guid, guid3, "testuser3 guid is different");

/* ========== 5 ========== */
testnum++;
testdesc = "(don't) modify user1";

// When newlogin.guid is blank, the GUID shouldn't be changed.
testuser1.guid = "";
storage.modifyLogin(testuser1, testuser1);

// Check it
do_check_eq(testuser1.guid, "", "caller's login shouldn't be modified");
logins = storage.findLogins({}, "http://testhost1", "", null);
do_check_eq(logins.length, 1, "expecting 1 login");
logins[0].QueryInterface(Ci.nsILoginMetaInfo);
do_check_eq(logins[0].guid, guid1, "checking guid1");

/* ========== 6 ========== */
testnum++;
testdesc = "modify user3";

// change the GUID to our known value
var propbag = Cc["@mozilla.org/hash-property-bag;1"].
              createInstance(Ci.nsIWritablePropertyBag);
propbag.setProperty("guid", guid3);
storage.modifyLogin(testuser3, propbag);

// Check it
logins = storage.findLogins({}, "http://testhost3", "", null);
do_check_eq(logins.length, 1, "expecting 1 login");
logins[0].QueryInterface(Ci.nsILoginMetaInfo);
do_check_eq(logins[0].guid, guid3, "checking guid3");

/* ========== 7 ========== */
testnum++;
testdesc = "try adding a duplicate guid";

var ex = null;
try {
    storage.addLogin(testuser2dupeguid);
} catch (e) {
    ex = e;
}
do_check_true(/specified GUID already exists/.test(ex), "ensuring exception thrown when adding duplicate GUID");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3]);

/* ========== 8 ========== */
testnum++;
testdesc = "try modifing to a duplicate guid";

propbag = Cc["@mozilla.org/hash-property-bag;1"].
          createInstance(Ci.nsIWritablePropertyBag);
propbag.setProperty("guid", testuser2dupeguid.guid);

ex = null;
try {
    storage.modifyLogin(testuser1, propbag);
} catch (e) {
    ex = e;
}
do_check_true(/specified GUID already exists/.test(ex), "ensuring exception thrown when modifying to duplicate GUID");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3]);


/* ========== 9 ========== */
testnum++;
testdesc = "check propertybag nulls/empty strings";

// Set formSubmitURL to a null, and usernameField to a empty-string.
do_check_eq(testuser3.formSubmitURL, "");
do_check_eq(testuser3.httpRealm, null);
do_check_eq(testuser3.usernameField, "put_user3_here");
propbag = Cc["@mozilla.org/hash-property-bag;1"].
          createInstance(Ci.nsIWritablePropertyBag);
propbag.setProperty("formSubmitURL", null);
propbag.setProperty("httpRealm", "newRealm");
propbag.setProperty("usernameField", "");

storage.modifyLogin(testuser3, propbag);

// Fixup testuser3 to match the new values.
testuser3.formSubmitURL = null;
testuser3.httpRealm = "newRealm";
testuser3.usernameField = "";
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3]);


/* ========== 10 ========== */
testnum++;
testdesc = "[reinit storage, look for expected guids]";

storage = LoginTest.reloadStorage(OUTDIR, "signons-unittest6.sqlite");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3]);

logins = storage.findLogins({}, "http://testhost1", "", null);
do_check_eq(logins.length, 1, "expecting 1 login");
logins[0].QueryInterface(Ci.nsILoginMetaInfo);
do_check_eq(logins[0].guid, guid1, "checking guid1");

logins = storage.findLogins({}, "http://testhost2", "", null);
do_check_eq(logins.length, 1, "expecting 1 login");
logins[0].QueryInterface(Ci.nsILoginMetaInfo);
do_check_eq(logins[0].guid, guid2, "checking guid2");

logins = storage.findLogins({}, "http://testhost3", null, "newRealm");
do_check_eq(logins.length, 1, "expecting 1 login");
logins[0].QueryInterface(Ci.nsILoginMetaInfo);
do_check_eq(logins[0].guid, guid3, "checking guid3");


/* ========== 11 ========== */
testnum++;
testdesc = "check values for v4 DB addLogin";

var timeuser1 = new nsLoginInfo();
timeuser1.init("http://time1", "", null, "timeuser1", "origpass1", "foo", "bar");

storage.addLogin(timeuser1);
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3, timeuser1]);

let matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
matchData.setProperty("hostname", "http://time1");
logins = storage.searchLogins({}, matchData);
do_check_eq(1, logins.length);

let tu1 = logins[0];
tu1.QueryInterface(Ci.nsILoginMetaInfo);
let time1 = tu1.timeCreated;

// Check that times and usage count were initialized properly.
do_check_true(LoginTest.is_about_now(time1));
do_check_eq(time1, tu1.timeLastUsed);
do_check_eq(time1, tu1.timePasswordChanged);
do_check_eq(1, tu1.timesUsed);


/* ========== 12 ========== */
testnum++;
testdesc = "check values for v4 DB addLogin part 2";

var timeuser2 = new nsLoginInfo();
timeuser2.init("http://time2", "", null, "timeuser2", "origpass2", "foo", "bar");
timeuser2.QueryInterface(Ci.nsILoginMetaInfo);
// This time pre-init the times and usage count.
timeuser2.timeCreated = 123;
timeuser2.timeLastUsed = 456;
timeuser2.timePasswordChanged = 789;
timeuser2.timesUsed = 42;

storage.addLogin(timeuser2);
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3, timeuser1, timeuser2]);

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
matchData.setProperty("hostname", "http://time2");
logins = storage.searchLogins({}, matchData);
do_check_eq(1, logins.length);
let tu2 = logins[0];

tu2.QueryInterface(Ci.nsILoginMetaInfo);

// Check that values we specified were set, instead of defaults.
do_check_eq(123, tu2.timeCreated);
do_check_eq(456, tu2.timeLastUsed);
do_check_eq(789, tu2.timePasswordChanged);
do_check_eq(42, tu2.timesUsed);


/* ========== 13 ========== */
testnum++;
testdesc = "check values for v4 DB modifyLogin";

let modData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
modData.setProperty("timesUsed", 8);

storage.modifyLogin(timeuser2, modData);

modData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
modData.setProperty("password", "newpass2");

storage.modifyLogin(timeuser2, modData);
timeuser2.password = "newpass2";

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
matchData.setProperty("hostname", "http://time2");
logins = storage.searchLogins({}, matchData);
do_check_eq(1, logins.length);
tu2 = logins[0];

tu2.QueryInterface(Ci.nsILoginMetaInfo);

// Check that values we specified were set, instead of defaults.
do_check_eq("newpass2", tu2.password);
do_check_eq(123, tu2.timeCreated);
do_check_eq(456, tu2.timeLastUsed);
do_check_true(LoginTest.is_about_now(tu2.timePasswordChanged));
do_check_eq(8, tu2.timesUsed);


/* ========== 14 ========== */
testnum++;
testdesc = "check values for v4 DB modifyLogin part 2";

modData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
modData.setProperty("timesUsedIncrement", 2);

storage.modifyLogin(timeuser2, modData);

modData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
modData.setProperty("password", "newpass2again\u0394\u00e8");
modData.setProperty("timePasswordChanged", 888); // pw change with specific time

storage.modifyLogin(timeuser2, modData);
timeuser2.password = "newpass2again\u0394\u00e8";

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
matchData.setProperty("hostname", "http://time2");
logins = storage.searchLogins({}, matchData);
do_check_eq(1, logins.length);
tu2 = logins[0];

tu2.QueryInterface(Ci.nsILoginMetaInfo);

// Check that values we specified were set, instead of defaults.
do_check_eq("newpass2again\u0394\u00e8", tu2.password);
do_check_eq(123, tu2.timeCreated);
do_check_eq(456, tu2.timeLastUsed);
do_check_eq(888, tu2.timePasswordChanged);
do_check_eq(10, tu2.timesUsed);


/* ========== 15 ========== */
testnum++;
testdesc = "check values for v4 DB modifyLogin part 3";

modData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
modData.setProperty("timeCreated",  5444333222111);
modData.setProperty("timeLastUsed", 22222);

storage.modifyLogin(timeuser2, modData);

matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
matchData.setProperty("hostname", "http://time2");
logins = storage.searchLogins({}, matchData);
do_check_eq(1, logins.length);
tu2 = logins[0];

tu2.QueryInterface(Ci.nsILoginMetaInfo);

// Check that values we specified were set, instead of defaults.
do_check_eq(5444333222111, tu2.timeCreated);
do_check_eq(22222, tu2.timeLastUsed);
do_check_eq(888, tu2.timePasswordChanged);
do_check_eq(10, tu2.timesUsed);

// some cleanup
storage.removeLogin(timeuser1);
storage.removeLogin(timeuser2);
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3]);



LoginTest.deleteFile(OUTDIR, "signons-unittest6.sqlite");

} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
};
