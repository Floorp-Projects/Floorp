/*
 * Test suite for storage-Legacy.js -- exercises reading from on-disk storage.
 *
 * This test interfaces directly with the legacy password storage module, by passing the normal
 * password manager usage.
 *
 */

var DIR_SERVICE = new Components.Constructor(
                    "@mozilla.org/file/directory_service;1", "nsIProperties");
var TESTDIR = (new DIR_SERVICE()).get("ProfD", Ci.nsIFile).path;

function initStorage(filename, storage, expectedError) {
	var e, caughtError = false;

    var file = do_get_file("toolkit/components/passwordmgr/test/unit/data/" + filename);

	try {
		storage.initWithFile(file.parent.path, file.leafName, null);
	} catch (e) { caughtError = true; var err = e;}

	if (expectedError) {
		if (!caughtError) { throw "Component should have returned an error (" + expectedError + "), but did not."; }

		if (!expectedError.test(err)) { throw "Expected error was of wrong type: expected (" + expectedError + "), got " + err; }

		// We got the expected error, so make a note in the test log. Don't Panic!
		dump("...that error was expected.\n\n");
	}

	return;
};


// Compare info from component to what we expected.
function checkStorageData(storage, ref_disabledHosts, ref_logins) {

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
var nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1", Components.interfaces.nsILoginInfo);
do_check_true(nsLoginInfo != null);

testuser1 = new nsLoginInfo;
testuser1.hostname      = "http://dummyhost.mozilla.org";
testuser1.formSubmitURL = "";
testuser1.username      = "dummydude";
testuser1.password      = "itsasecret";
testuser1.usernameField = "put_user_here";
testuser1.passwordField = "put_pw_here";
testuser1.httpRealm     = null;

testuser2 = new nsLoginInfo;
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

var storage = Cc["@mozilla.org/login-manager/storage/legacy;1"].createInstance(Ci.nsILoginManagerStorage);
if (!storage) throw "Couldn't create storage instance.";


dump("/* ========== 2 ========== */\n");
testnum++;
testdesc = "[ensuring file doesn't exist]";
// XXX skip this test
if (0) {
  var filename="this-file-does-not-exist-"+Math.floor(Math.random() * 10000);
  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(TESTDIR);
  file.append(filename);
  var exists = file.exists();
  if (exists) {
        // Go ahead and remove the file, so that this failure doesn't repeat itself w/o intervention.
        file.remove(false);
        do_check_false(exists);
  }

  testdesc = "Initialize with a non-existant data file";

  initStorage(filename, storage, null);
  checkStorageData(storage, [], []);

  file.remove(false);
}

dump("/* ========== 3 ========== */\n");
testnum++;
testdesc = "Initialize with signons-00.txt (a zero-length file)";

initStorage("signons-00.txt", storage, /invalid file header/);
checkStorageData(storage, [], []);


/* ========== 4 ========== */
testnum++;
testdesc = "Initialize with signons-01.txt (bad file header)";

initStorage("signons-01.txt", storage, /invalid file header/);
checkStorageData(storage, [], []);


/* ========== 5 ========== */
testnum++;
testdesc = "Initialize with signons-02.txt (valid, but empty)";

initStorage("signons-02.txt", storage, null);
checkStorageData(storage, [], []);


/* ========== 6 ========== */
testnum++;
testdesc = "Initialize with signons-03.txt (1 disabled, 0 logins)";

initStorage("signons-03.txt", storage, null);

disabledHosts = ["http://www.disabled.com"];
checkStorageData(storage, disabledHosts, []);


/* ========== 7 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 0 logins, extra '.')";
// XXX I'm not sure if the extra dot is possible with any released code, but
// it seems likely that a 3rd party program might do this, since it's
// consistant with how signons.txt looks when there's at least 1 login.
// So, well test it anyway to be sure we don't accidently break this hypothetical case.

initStorage("signons-04.txt", storage, null);

disabledHosts = ["http://www.disabled.com"];
checkStorageData(storage, disabledHosts, []);


/* ========== 8 ========== */
testnum++;
testdesc = "Initialize with signons-05.txt (0 disabled, 1 login)";

initStorage("signons-05.txt", storage, null);

logins = [testuser1];
checkStorageData(storage, [], logins);


/* ========== 9 ========== */
testnum++;
testdesc = "Initialize with signons-06.txt (1 disabled, 1 login)";

initStorage("signons-06.txt", storage, null);

disabledHosts = ["https://www.site.net"];
logins = [testuser1];
checkStorageData(storage, disabledHosts, logins);


/* ========== 10 ========== */
testnum++;
testdesc = "Initialize with signons-07.txt (0 disabled, 2 logins for same host)";

initStorage("signons-07.txt", storage, null);

logins = [testuser1, testuser2];
checkStorageData(storage, [], logins);


/* ========== 11 ========== */
testnum++;
testdesc = "Initialize with signons-08.txt (1000 disabled, 1000 logins)";

initStorage("signons-08.txt", storage, null);

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
checkStorageData(storage, disabledHosts, logins);


// XXX more test ideas...
// * old/new formats (header, actionURL)
// * HTTP realm testing
// * various newline types for cross-platform compat. checking

} catch (e) {
	throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e; }
};


