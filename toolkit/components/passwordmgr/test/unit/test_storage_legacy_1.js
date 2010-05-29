/*
 * Test suite for storage-Legacy.js -- exercises reading from on-disk storage.
 *
 * This test interfaces directly with the legacy password storage module,
 * bypassing the normal password manager usage.
 *
 */

const STORAGE_TYPE = "legacy";

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

testdesc = "Initialize with a nonexistent data file";

storage = LoginTest.initStorage(OUTDIR, filename);

LoginTest.checkStorageData(storage, [], []);

if (file.exists())
    file.remove(false);

/* ========== 3 ========== */
testnum++;
testdesc = "Initialize with signons-00.txt (a zero-length file)";

storage = LoginTest.initStorage(INDIR, "signons-00.txt",
                     null, null, /invalid file header/);
LoginTest.checkStorageData(storage, [], []);


/* ========== 4 ========== */
testnum++;
testdesc = "Initialize with signons-01.txt (bad file header)";

storage = LoginTest.initStorage(INDIR, "signons-01.txt",
                     null, null, /invalid file header/);
LoginTest.checkStorageData(storage, [], []);


/* ========== 5 ========== */
testnum++;
testdesc = "Initialize with signons-02.txt (valid, but empty)";

storage = LoginTest.initStorage(INDIR, "signons-02.txt");
LoginTest.checkStorageData(storage, [], []);


/* ========== 6 ========== */
testnum++;
testdesc = "Initialize with signons-03.txt (1 disabled, 0 logins)";

storage = LoginTest.initStorage(INDIR, "signons-03.txt");
LoginTest.checkStorageData(storage, ["http://www.disabled.com"], []);


/* ========== 7 ========== */
testnum++;
testdesc = "Initialize with signons-04.txt (1 disabled, 0 logins, extra '.')";

// Mozilla code should never have generated the extra ".", but it's possible
// someone writing an external utility might have generated it, since it
// would seem consistant with the format.
storage = LoginTest.initStorage(INDIR, "signons-04.txt");
LoginTest.checkStorageData(storage, ["http://www.disabled.com"], []);


/* ========== 8 ========== */
testnum++;
testdesc = "Initialize with signons-05.txt (0 disabled, 1 login)";

storage = LoginTest.initStorage(INDIR, "signons-05.txt");
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


/* ========== 9 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login)";

storage = LoginTest.initStorage(INDIR, "signons-06.txt");
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);


/* ========== 10 ========== */
testnum++;
testdesc = "Initialize with signons-07.txt (0 disabled, 2 logins on same host)";

storage = LoginTest.initStorage(INDIR, "signons-07.txt");
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
testdesc = "Initialize with signons-08.txt (500 disabled, 500 logins)";

storage = LoginTest.initStorage(INDIR, "signons-08.txt");

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


/* ========== 12 ========== */
testnum++;
testdesc = "Initialize with signons-2c-01.txt";
// This test confirms that when upgrading from an entry that does not have
// a formSubmitURL stored, we correctly set the new formSubmitURL to
// null or "", depending on if it's a form login or protocol login.

// First with a form login (formSubmitURL should be "")
storage = LoginTest.initStorage(INDIR, "signons-2c-01.txt");
LoginTest.checkStorageData(storage, [], [testuser1]);

// Then with a protocol login (formSubmitURL should be null)
testdesc = "Initialize with signons-2c-02.txt";
var testuser3 = new nsLoginInfo;
testuser3.init("http://dummyhost.mozilla.org", null, "Some Realm",
    "dummydude", "itsasecret", "", "");
storage = LoginTest.initStorage(INDIR, "signons-2c-02.txt");
LoginTest.checkStorageData(storage, [], [testuser3]);


/* ========== 13 ========== */
testnum++;
testdesc = "Initialize with signons-2c-03.txt";
// Make sure that when we read a blank HTTP realm, it's reset to
// the hostname and the formSubmitURL is null (not blank).
testuser3.init("http://dummyhost.mozilla.org", null,
               "http://dummyhost.mozilla.org",
               "dummydude", "itsasecret", "", "");
storage = LoginTest.initStorage(INDIR, "signons-2c-03.txt");
LoginTest.checkStorageData(storage, [], [testuser3]);

/* ========== 14 ========== */
testnum++;
testdesc = "Initialize with signons-2d-01.txt";
// This test confirms that when reading a 2D entry with a blank
// formSubmitURL line, the parser sets formSubmitURL to null or ""
// depending on if it's a form login or protocol login.

// First with a form login
storage = LoginTest.initStorage(INDIR, "signons-2d-01.txt");
LoginTest.checkStorageData(storage, [], [testuser1]);

// Then with a protocol login
testdesc = "Initialize with signons-2d-02.txt";
testuser3.init("http://dummyhost.mozilla.org", null, "Some Realm",
    "dummydude", "itsasecret", "", "");
storage = LoginTest.initStorage(INDIR, "signons-2d-02.txt");
LoginTest.checkStorageData(storage, [], [testuser3]);


/* ========== 15 ========== */
testnum++;
testdesc = "Initialize with signons-2d-03.txt";
// This test confirms that when upgrading a 2D entry with HTTP[S] protocol
// logins on the default port, that we correctly update the form and drop
// the port number. EG:
// www.site.com:80 --> http://www.site.com

testuser1.init("http://dummyhost80.mozilla.org", null, "Some Realm",
    "dummydude", "itsasecret", "", "");
testuser2.init("https://dummyhost443.mozilla.org", null, "Some Realm",
    "dummydude", "itsasecret", "", "");

storage = LoginTest.initStorage(INDIR, "signons-2d-03.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);


/* ========== 16 ========== */
testnum++;
testdesc = "Initialize with signons-2d-04.txt";
// This test confirms that when upgrading a 2D entry with a protocol
// logins on a nonstandard port, that we add both http and https
// versions of the login.
// site.com:8080 --> http://site.com:8080, https://site.com:8080

testuser1.init("http://dummyhost8080.mozilla.org:8080", null, "Some Realm",
    "dummydude", "itsasecret", "", "");
testuser2.init("https://dummyhost8080.mozilla.org:8080", null, "Some Realm",
    "dummydude", "itsasecret", "", "");

storage = LoginTest.initStorage(INDIR, "signons-2d-04.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);


/* ========== 17 ========== */
testnum++;
testdesc = "Initialize with signons-2d-05.txt";
// This test confirms that when upgrading a 2D entry with a blank
// HTTP realm, we set the realm to the hostname.
// foo.com:80 () --> http://foo.com (http://foo.com)

testuser1.init("http://dummyhost80.mozilla.org", null,
               "http://dummyhost80.mozilla.org",
               "dummydude", "itsasecret", "", "");
testuser2.init("https://dummyhost443.mozilla.org", null,
               "https://dummyhost443.mozilla.org",
               "dummydude", "itsasecret", "", "");

storage = LoginTest.initStorage(INDIR, "signons-2d-05.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);


// And again, with a single entry that was cloned
testdesc = "Initialize with signons-2d-06.txt";
testuser1.init("http://dummyhost8080.mozilla.org:8080", null,
               "http://dummyhost8080.mozilla.org:8080",
               "dummydude", "itsasecret", "", "");
testuser2.init("https://dummyhost8080.mozilla.org:8080", null,
               "https://dummyhost8080.mozilla.org:8080",
               "dummydude", "itsasecret", "", "");

storage = LoginTest.initStorage(INDIR, "signons-2d-06.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);


/* ========== 18 ========== */
testnum++;
testdesc = "Initialize with signons-2d-07.txt";
// Form logins could have been saved with the port number explicitly
// specified in the hostname or formSubmitURL. Make sure it gets
// stripped when it's the default value.

testuser1.init("http://dummyhost80.mozilla.org",
               "http://dummyhost80.mozilla.org", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");
testuser2.init("https://dummyhost443.mozilla.org",
               "https://dummyhost443.mozilla.org", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");
// non-standard port, so it shouldn't get stripped.
testuser3.init("http://dummyhost8080.mozilla.org:8080",
               "http://dummyhost8080.mozilla.org:8080", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");
storage = LoginTest.initStorage(INDIR, "signons-2d-07.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3]);


/* ========== 19 ========== */
testnum++;
testdesc = "Initialize with signons-2d-08.txt";
// Bug 396316: Non-HTTP[S] hostnames were stored the same way for both forms 
// and protocol logins. If the login looks like it's not a form login (no
// form field names, no action URL), then assign it a default realm.

testuser1.init("ftp://protocol.mozilla.org", null,
               "ftp://protocol.mozilla.org",
               "dummydude", "itsasecret", "", "");
testuser2.init("ftp://form.mozilla.org", "", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");
testuser3.init("ftp://form2.mozilla.org", "http://cgi.mozilla.org", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");

storage = LoginTest.initStorage(INDIR, "signons-2d-08.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2, testuser3]);


/* ========== 20 ========== */
testnum++;
testdesc = "Initialize with signons-2d-09.txt";
// Logins stored when signing into, say, an FTP server via a URL with a
// username (ftp://user@ftp.site.com) were stored with a blank encrypted
// username, and the actual username in the hostname. Test to make sure we
// move it back to the encrypted username field. If the username was saved as
// part of a form, just drop it. (eg, ftp://ftpuser@site.com/form.html)

testuser1.init("ftp://protocol.mozilla.org", null, "ftp://protocol.mozilla.org",
               "urluser", "itsasecret", "", "");
testuser2.init("ftp://form.mozilla.org", "", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");
storage = LoginTest.initStorage(INDIR, "signons-2d-09.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);


/* ========== 21 ========== */
testnum++;
testdesc = "Initialize with signons-2d-10.txt";
// Extensions like the eBay Companion just use an arbitrary string for the
// hostname. We don't want to change the format of these logins.

testuser1.init("eBay.companion.paypal.guard", "", null,
               "p", "paypalpass", "", "");
testuser2.init("eBay.companion.ebay.guard", "", null,
               "p", "ebaypass", "", "");
storage = LoginTest.initStorage(INDIR, "signons-2d-10.txt");
LoginTest.checkStorageData(storage, [], [testuser1, testuser2]);


/* ========== 22 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login); test removeLogin";

testuser1.init("http://dummyhost.mozilla.org", "", null,
    "dummydude", "itsasecret", "put_user_here", "put_pw_here");
testuser2.init("http://dummyhost.mozilla.org", "", null,
    "dummydude2", "itsasecret2", "put_user2_here", "put_pw2_here");

storage = LoginTest.initStorage(INDIR, "signons-06.txt", OUTDIR, "signons-06-2.txt");
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);

testdesc = "test removeLogin";
storage.removeLogin(testuser1);
LoginTest.checkStorageData(storage, ["https://www.site.net"], []);


/* ========== 23 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login); test modifyLogin";

storage = LoginTest.initStorage(INDIR, "signons-06.txt",  OUTDIR, "signons-06-3.txt");
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser1]);

testdesc = "test modifyLogin";
storage.modifyLogin(testuser1, testuser2);
LoginTest.checkStorageData(storage, ["https://www.site.net"], [testuser2]);


/*
 * ---------------------- Bug 427033 ----------------------
 * Check migration of logins stored with a JS formSubmitURL
 */


/* ========== 24 ========== */
testnum++;

testdesc = "checking import of JS formSubmitURL entries"

testuser1.init("http://jstest.site.org", "javascript:", null,
               "dummydude", "itsasecret", "put_user_here", "put_pw_here");
storage = LoginTest.initStorage(INDIR, "signons-427033-1.txt",
                               OUTDIR, "output-427033-1.txt");
LoginTest.checkStorageData(storage, [], [testuser1]);

testdesc = "[flush and reload for verification]"
storage = LoginTest.reloadStorage(OUTDIR, "output-427033-1.txt");
LoginTest.checkStorageData(storage, [], [testuser1]);



} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
};
