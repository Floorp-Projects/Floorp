/*
 * Test suite for storage-Legacy.js -- various bug fixes.
 *
 * This test interfaces directly with the legacy login storage module,
 * bypassing the normal login manager usage.
 *
 */


const STORAGE_TYPE = "legacy";

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
storage = LoginTest.initStorage(INDIR, "signons-380961-1.txt",
                               OUTDIR, "output-380961-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-380961-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1]);

/* ========== 3 ========== */
testnum++;

testdesc = "testing import of multiple mime-64 entries for a host"
storage = LoginTest.initStorage(INDIR, "signons-380961-2.txt",
                               OUTDIR, "output-380961-2.txt");
LoginTest.checkStorageData(storage, [], [dummyuser2, dummyuser3]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-380961-2.txt");
LoginTest.checkStorageData(storage, [], [dummyuser2, dummyuser3]);

/* ========== 4 ========== */
testnum++;

testdesc = "testing import of mixed encrypted and mime-64 entries."
storage = LoginTest.initStorage(INDIR, "signons-380961-3.txt",
                               OUTDIR, "output-380961-3.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-380961-3.txt");
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
storage = LoginTest.initStorage(INDIR, "signons-381262.txt",
                               OUTDIR, "output-381262-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser4]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-381262-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser4]);


/* ========== 7 ========== */
testnum++;

testdesc = "testing storage of non-ascii username and password."
storage = LoginTest.initStorage(INDIR, "signons-empty.txt",
                               OUTDIR, "output-381262-2.txt");
LoginTest.checkStorageData(storage, [], []);
storage.addLogin(dummyuser4);
LoginTest.checkStorageData(storage, [], [dummyuser4]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-381262-2.txt");
LoginTest.checkStorageData(storage, [], [dummyuser4]);


/*
 * ---------------------- Bug 400751 ----------------------
 * Migrating from existing mime64 encoded format causes
 * errors in storage legacy's code
 */


/* ========== 8 ========== */
testnum++;

testdesc = "checking double reading of mime64-obscured entries";
storage = LoginTest.initStorage(INDIR, "signons-380961-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1]);

testdesc = "checking double reading of mime64-obscured entries part 2";
LoginTest.checkStorageData(storage, [], [dummyuser1]);

/* ========== 9 ========== */
testnum++;

testdesc = "checking correct storage of mime64 converted entries";
storage = LoginTest.initStorage(INDIR, "signons-380961-1.txt",
                               OUTDIR, "output-400751-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1]);
LoginTest.checkStorageData(storage, [], [dummyuser1]);
storage.addLogin(dummyuser2); // trigger a write
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2]);

testdesc = "[flush and reload for verification]";
storage = LoginTest.reloadStorage(OUTDIR, "output-400751-1.txt");
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2]);

/*
 * ---------------------- Bug 394610 ----------------------
 * Ensure input which might mess with the format or structure of
 * the store file is appropriately filtered.
 */

/* ========== 10 ========== */
testnum++;

function tryAddUser(storage, aUser, aExpectedError) {
    var err = null;
    try {
        storage.addLogin(aUser);
    } catch (e) {
        err = e;
    }

    LoginTest.checkExpectedError(aExpectedError, err);
}

testdesc = "preparting to try logins with bogus values";
storage = LoginTest.initStorage(INDIR, "signons-empty.txt",
                               OUTDIR, "output-394610-1.txt");
LoginTest.checkStorageData(storage, [], []);


var failUser = Cc["@mozilla.org/login-manager/loginInfo;1"].
               createInstance(Ci.nsILoginInfo);
// init with legal values, we'll change on-the-fly
failUser.init("http://failure.site.org",
    "http://failure.site.org", null,
    "username", "password", "uname", "pword");


testdesc = "storing data values with embedded newlines."


// newline in "never for this site" address.
var failHost = "http://new\nline.never.net";
var error = null;
try {
    storage.setLoginSavingEnabled(failHost, false);
} catch (e) {
    error = e;
}
LoginTest.checkExpectedError(/Invalid hostname/, error);

// CR in "never for this site" address.
failHost = "http://new\rline.never.net";
error = null;
try {
    storage.setLoginSavingEnabled(failHost, false);
} catch (e) {
    error = e;
}
LoginTest.checkExpectedError(/Invalid hostname/, error);


// check hostname
failUser.hostname = "http://fail\nure.site.org";
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.hostname = "http://fail\rure.site.org";
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.hostname = "http://failure.site.org";


// check httpRealm and formSubmitURL
failUser.httpRealm = "http://fail\nure.site.org";
failUser.formSubmitURL = null;
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.httpRealm = "http://fail\rure.site.org";
failUser.formSubmitURL = null;
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.formSubmitURL = "http://fail\nure.site.org";
failUser.httpRealm = null;
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.formSubmitURL = "http://fail\rure.site.org";
failUser.httpRealm = null;
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.formSubmitURL = "http://failure.site.org";


// check usernameField
failUser.usernameField = "u\nname";
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.usernameField = "u\rname";
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.usernameField = "uname";


// check passwordField
failUser.passwordField = "p\nword";
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.passwordField = "p\rword";
tryAddUser(storage, failUser, /login values can't contain newlines/);

failUser.passwordField = "pword";


// check username and password, which are OK with embedded newlines.
failUser.username = "user\r\nname";
failUser.password = "pass\r\nword";
tryAddUser(storage, failUser, null);

var numLines = LoginTest.countLinesInFile(OUTDIR, "output-394610-1.txt");
do_check_eq(numLines, 10);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-394610-1.txt");
LoginTest.checkStorageData(storage, [], [failUser]);

failUser.username = "username";
failUser.password = "password";


/* ========== 11 ========== */
testnum++;

testdesc = "storing data values with special period-only value"
storage = LoginTest.initStorage(INDIR, "signons-empty.txt",
                               OUTDIR, "output-394610-2.txt");
LoginTest.checkStorageData(storage, [], []);


failHost = ".";
error = null;
try {
    storage.setLoginSavingEnabled(failHost, false);
} catch (e) {
    error = e;
}
LoginTest.checkExpectedError(/Invalid hostname/, error);



// check usernameField
failUser.usernameField = ".";
tryAddUser(storage, failUser, /login values can't be periods/);
failUser.usernameField = "uname";

// check formSubmitURL
failUser.usernameField = ".";
tryAddUser(storage, failUser, /login values can't be periods/);
failUser.formSubmitURL = "http://failure.site.org";

testdesc = "check added data"
LoginTest.checkStorageData(storage, [], []);
var numLines = LoginTest.countLinesInFile(OUTDIR, "output-394610-2.txt");
do_check_eq(numLines, 2);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-394610-2.txt");
LoginTest.checkStorageData(storage, [], []);


/* ========== 12 ========== */
testnum++;

testdesc = "create logins with parens in host/httpRealm"

storage = LoginTest.initStorage(INDIR, "signons-empty.txt",
                               OUTDIR, "output-394610-3.txt");
LoginTest.checkStorageData(storage, [], []);

var parenUser1 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var parenUser2 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var parenUser3 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var parenUser4 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var parenUser5 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var parenUser6 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var parenUser7 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var parenUser8 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var parenUser9 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);

// realm as "(realm", "realm)", "(realm)", ")realm("
parenUser1.init("http://parens.site.org", null, "(realm",
    "user1", "pass1", "uname", "pword");
parenUser2.init("http://parens.site.org", null, "realm)",
    "user2", "pass2", "uname", "pword");
parenUser3.init("http://parens.site.org", null, "(realm)",
    "user3", "pass3", "uname", "pword");
parenUser4.init("http://parens.site.org", null, ")realm(",
    "user4", "pass4", "uname", "pword");

// hostname as "xx(xx", "xx)xx", "xx()xx", "xx)(xx"
parenUser5.init("http://parens(yay.site.org", null, "realm",
    "user5", "pass5", "uname", "pword");
parenUser6.init("http://parens)yay.site.org", null, "realm",
    "user6", "pass6", "uname", "pword");
parenUser7.init("http://parens(yay).site.org", null, "realm",
    "user7", "pass7", "uname", "pword");
parenUser8.init("http://parens)yay(.site.org", null, "realm",
    "user8", "pass8", "uname", "pword");

// "xx (xx" as a special case
parenUser9.init("http://parens (.site.org", null, "realm",
    "user9", "pass9", "uname", "pword");

testdesc = "add logins with parens in host/httpRealm"
tryAddUser(storage, parenUser1, null);
tryAddUser(storage, parenUser2, null);
tryAddUser(storage, parenUser3, null);
tryAddUser(storage, parenUser4, null);
tryAddUser(storage, parenUser5, null);
tryAddUser(storage, parenUser6, null);
tryAddUser(storage, parenUser7, null);
tryAddUser(storage, parenUser8, null);

// we expect this one to fail.
tryAddUser(storage, parenUser9, /bad parens in hostname/);

var parenLogins = [
    parenUser1, parenUser2, parenUser3, parenUser4,
    parenUser5, parenUser6, parenUser7, parenUser8
    ];

testdesc = "check added data"
LoginTest.checkStorageData(storage, [], parenLogins);
var numLines = LoginTest.countLinesInFile(OUTDIR, "output-394610-3.txt");
do_check_eq(numLines, 66);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-394610-3.txt");
LoginTest.checkStorageData(storage, [], parenLogins);


/* ========== 13 ========== */
testnum++;

testdesc = "storing data values with embedded nulls."

// sanity check
do_check_eq( "foo\0bar", "foo\0bar");
do_check_neq("foo\0bar", "foobar");

storage = LoginTest.initStorage(INDIR, "signons-empty.txt",
                               OUTDIR, "output-394610-4.txt");
LoginTest.checkStorageData(storage, [], []);

var nullUser = Cc["@mozilla.org/login-manager/loginInfo;1"].
               createInstance(Ci.nsILoginInfo);

nullUser.init("http://null.site.org",
    "http://null.site.org", null,
    "username", "password", "usernull", "passnull");

// null in "never for this site" address.
var nullHost = "http://never\0X.sit.org";
error = null;
try {
    storage.setLoginSavingEnabled(nullHost, false);
} catch (e) {
    error = e;
}
LoginTest.checkExpectedError(/Invalid hostname/, error);


// check hostname
nullUser.hostname = "http://null\0X.site.org";
tryAddUser(storage, nullUser, /login values can't contain nulls/);
nullUser.hostname = "http://null.site.org";


// check httpRealm and formSubmitURL
nullUser.httpRealm = "http://null\0X.site.org";
nullUser.formSubmitURL = null;
tryAddUser(storage, nullUser, /login values can't contain nulls/);

nullUser.formSubmitURL = "http://null\0X.site.org";
nullUser.httpRealm = null;
tryAddUser(storage, nullUser, /login values can't contain nulls/);

nullUser.formSubmitURL = "http://null.site.org";


// check usernameField
nullUser.usernameField = "usernull\0X";
tryAddUser(storage, nullUser, /login values can't contain nulls/);
nullUser.usernameField = "usernull";

// check usernameField with a special case value
nullUser.usernameField = ".\0";
tryAddUser(storage, nullUser, /login values can't contain nulls/);
nullUser.usernameField = "usernull";

// check passwordField
nullUser.passwordField = "passnull\0X";
tryAddUser(storage, nullUser, /login values can't contain nulls/);
nullUser.passwordField = "passnull";


// check username with null
nullUser.username = "user\0name";
tryAddUser(storage, nullUser, /login values can't contain nulls/);
nullUser.username = "username";

// check password with null
nullUser.password = "pass\0word";
tryAddUser(storage, nullUser, /login values can't contain nulls/);
nullUser.password = "password";


// Final sanity check, to make sure we didn't store anything unexpected.
LoginTest.checkStorageData(storage, [], []);
var numLines = LoginTest.countLinesInFile(OUTDIR, "output-394610-4.txt");
do_check_eq(numLines, 2);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-394610-4.txt");
LoginTest.checkStorageData(storage, [], []);


/*
 * ---------------------- Bug 449701 ----------------------
 * Ensure changes to login objects given to / obtained from
 * the storage module don't affect the internal storage.
 */

/* ========== 14 ========== */
testnum++;
testdesc = "ensure internal login objects not shared with callers."

storage = LoginTest.initStorage(INDIR, "signons-empty.txt",
                               OUTDIR, "output-449701.txt");
LoginTest.checkStorageData(storage, [], []);

// dummyuser1 == dummyuser2
dummyuser1.init("http://dummyhost.mozilla.org", "", null,
    "testuser1", "testpass1", "put_user_here", "put_pw_here");
dummyuser2.init("http://dummyhost.mozilla.org", "", null,
    "testuser1", "testpass1", "put_user_here", "put_pw_here");


// Add a login, modify it, make sure orginal values are still stored.
storage.addLogin(dummyuser1);
LoginTest.checkStorageData(storage, [], [dummyuser2]);
dummyuser1.usernameField = "ohnoes";
LoginTest.checkStorageData(storage, [], [dummyuser2]);

// Get a stored login, modify it, make sure the stored login wasn't changed.
var logins = storage.getAllLogins();
do_check_eq(logins.length, 1);
var obtainedLogin1 = logins[0];
obtainedLogin1.usernameField = "ohnoes";

logins = storage.getAllLogins();
var obtainedLogin2 = logins[0];

do_check_neq(obtainedLogin1.usernameField, obtainedLogin2.usernameField);


/*
 * ---------------------- Bug 451155 ----------------------
 * Ensure that we don't mangle strings when then contain
 * UCS2 characters above U+00FF.
 */

/* ========== 15 ========== */
testnum++;
testdesc = "ensure UCS2 strings don't get mangled."

storage = LoginTest.initStorage(INDIR, "signons-empty.txt",
                               OUTDIR, "output-451155.txt");
LoginTest.checkStorageData(storage, [], []);

var testString = String.fromCharCode(355, 277, 349, 357, 533, 537, 101, 345, 185);

var utfHost = "http://" + testString + ".org";
var utfUser1 = Cc["@mozilla.org/login-manager/loginInfo;1"].
               createInstance(Ci.nsILoginInfo);
var utfUser2 = Cc["@mozilla.org/login-manager/loginInfo;1"].
               createInstance(Ci.nsILoginInfo);

utfUser1.init("http://" + testString + ".org",
    "http://" + testString + ".org", null,
    testString, testString, testString, testString);
utfUser2.init("http://realm.check.net", null, "realm " + testString + " test",
    "user", "pass", "", "");

storage.addLogin(utfUser1);
storage.addLogin(utfUser2);
storage.setLoginSavingEnabled(utfHost, false);

LoginTest.checkStorageData(storage, [utfHost], [utfUser1, utfUser2]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-451155.txt");
LoginTest.checkStorageData(storage, [utfHost], [utfUser1, utfUser2]);


/*
 * ---------------------- Bug 454708 ----------------------
 * Check that previous saved entries that are not valid UTF8
 * are read without blowing up.
 */

/* ========== 16 ========== */
testnum++;
testdesc = "ensure bogus UTF8 strings don't cause failures."

var badHost = "https://FcK" + String.fromCharCode(0x8a) + ".jp";
var bad8User = Cc["@mozilla.org/login-manager/loginInfo;1"].
               createInstance(Ci.nsILoginInfo);
bad8User.init(badHost, badHost, null,
              "dummydude", "itsasecret", "put_user_here", "put_pw_here");

storage = LoginTest.initStorage(INDIR, "signons-454708.txt",
                               OUTDIR, "output-454708.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

// The output file should contain valid UTF8 now, but the resulting
// JS string value remains the same.

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-454708.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);


/*
 * ---------------------- Bug 457358 ----------------------
 * need to reset UTF8 converter after it encounters invalid input
 * (pwmgr problems in FF3.0.3)
 */

/* ========== 17 ========== */
testnum++;
testdesc = "ensure UTF8 converter isn't left in bad state."

var utfRealm = "Acc" +
               String.fromCharCode(0xe8) +
               "s reserv" +
               String.fromCharCode(0xe9);
bad8User.init("https://bugzilla.mozilla.org", null, utfRealm,
            "dummydude", "itsasecret", "", "");

storage = LoginTest.initStorage(INDIR, "signons-457358-1.txt",
                               OUTDIR, "output-457358-1.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

// The output file should contain valid UTF8 now, but the resulting
// JS string value remains the same.

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-457358-1.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

/* ========== 18 ========== */
testnum++;
testdesc = "ensure UTF8 converter isn't left in bad state."

// The username field here is "Acc" + String.fromCharCode(0xe8), but the last
// character is invalid UTF8 -- it's the beginning of a multibyte sequence,
// but at the end of the input. The unicode converter silently truncates the
// string, and will throw when we feed it the next chunk of input (the
// encrypted username)
//
// Test for an expected login -- everthing fine except for the truncated field
// name (which we don't use anyway)
bad8User.init("https://bugzilla.mozilla.org", "https://bugzilla.mozilla.org", null,
            "dummydude", "itsasecret", "Acc", "pass");

storage = LoginTest.initStorage(INDIR, "signons-457358-2.txt",
                               OUTDIR, "output-457358-2.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

// The output file should contain valid UTF8 now, but the resulting
// JS string value remains the same.

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-457358-2.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

/* ========== 19 ========== */
testnum++;
testdesc = "ensure UTF8 converter isn't left in bad state."

// As with previous test, but triggered with both field names.

bad8User.init("https://bugzilla.mozilla.org", "https://bugzilla.mozilla.org", null,
            "dummydude", "itsasecret", "u-Acc", "p-Acc");

storage = LoginTest.initStorage(INDIR, "signons-457358-3.txt",
                               OUTDIR, "output-457358-3.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

// The output file should contain valid UTF8 now, but the resulting
// JS string value remains the same.

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-457358-3.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

/* ========== 20 ========== */
testnum++;
testdesc = "ensure UTF8 converter isn't left in bad state."

// Last character in the realm, this time. Not trunated, because the
// "http://site.com (realm)" format means there's always a trailing character,
// so the conversino throws.

bad8User.init("https://bugzilla.mozilla.org", null, "Acc" + String.fromCharCode(0xe8),
            "dummydude", "itsasecret", "", "");

storage = LoginTest.initStorage(INDIR, "signons-457358-4.txt",
                               OUTDIR, "output-457358-4.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

// The output file should contain valid UTF8 now, but the resulting
// JS string value remains the same.

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-457358-4.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

/* ========== 21 ========== */
testnum++;
testdesc = "ensure UTF8 converter isn't left in bad state."

// Like last test, but try adding and removing a login too.

bad8User.init("https://bugzilla.mozilla.org", null, "Acc" + String.fromCharCode(0xe8),
            "dummydude", "itsasecret", "", "");

storage = LoginTest.initStorage(INDIR, "signons-457358-4.txt",
                               OUTDIR, "output-457358-4b.txt");

var anotherUser = Cc["@mozilla.org/login-manager/loginInfo;1"].
              createInstance(Ci.nsILoginInfo);
anotherUser.init("http://mozilla.org", null,
                 String.fromCharCode(0xe8) + "xtra user " + String.fromCharCode(0x0163) + "est",
                 "dummydude", "itsasecret", "", "");

storage.addLogin(anotherUser);
LoginTest.checkStorageData(storage, [], [bad8User, anotherUser]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-457358-4b.txt");
LoginTest.checkStorageData(storage, [], [bad8User, anotherUser]);

storage.removeLogin(anotherUser);
LoginTest.checkStorageData(storage, [], [bad8User]);

testdesc = "[flush and reload for verification 2]"
storage = LoginTest.reloadStorage(OUTDIR, "output-457358-4b.txt");
LoginTest.checkStorageData(storage, [], [bad8User]);

/* ========== 22 ========== */
testnum++;
testdesc = "ensure UTF8 converter isn't left in bad state."

// The first login's username (plaintext) is invalid UTF8. It's handled as a
// bad decryption so we don't see the login, but make sure the other login in
// the file is ok.

// This is the first login:
//bad8User.init("https://www.google.com", "https://www.google.com", null,
//              "Acc" + String.fromCharCode(0xe8) + "ss", "passw", "un", "pw");
anotherUser.init("https://bugzilla.mozilla.org", null, "extra user test",
                 "dummydude", "itsasecret", "", "");

storage = LoginTest.initStorage(INDIR, "signons-457358-5.txt",
                               OUTDIR, "output-457358-5.txt");
LoginTest.checkStorageData(storage, [], [anotherUser]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-457358-5.txt");
LoginTest.checkStorageData(storage, [], [anotherUser]);

/* ========== end ========== */
} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

};
