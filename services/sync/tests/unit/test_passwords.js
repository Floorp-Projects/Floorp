load("fake_login_manager.js");

var loginMgr = new FakeLoginManager(fakeSampleLogins);

// The JS module we're testing, with all members exposed.
var passwords = loadInSandbox("resource://weave/engines/passwords.js");

function test_hashLoginInfo_works() {
  var fakeUserHash = passwords._hashLoginInfo(fakeSampleLogins[0]);
  do_check_eq(typeof fakeUserHash, 'string');
  do_check_eq(fakeUserHash.length, 40);
}

function test_synccore_itemexists_works() {
  var fakeUserHash = passwords._hashLoginInfo(fakeSampleLogins[0]);
  var psc = new passwords.PasswordSyncCore();
  do_check_false(psc._itemExists("invalid guid"));
  do_check_true(psc._itemExists(fakeUserHash));
}
