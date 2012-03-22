/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/identity.js");

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Identity").level =
    Log4Moz.Level.Trace;

  run_next_test();
}

add_test(function test_username_from_account() {
  _("Ensure usernameFromAccount works properly.");

  do_check_eq(Identity.usernameFromAccount(null), null);
  do_check_eq(Identity.usernameFromAccount("user"), "user");
  do_check_eq(Identity.usernameFromAccount("User"), "user");
  do_check_eq(Identity.usernameFromAccount("john@doe.com"),
                                           "7wohs32cngzuqt466q3ge7indszva4of");

  run_next_test();
});

add_test(function test_account_username() {
  _("Ensure the account and username attributes work properly.");

  _("Verify initial state");
  do_check_eq(Svc.Prefs.get("account"), undefined);
  do_check_eq(Svc.Prefs.get("username"), undefined);
  do_check_eq(Identity.account, null);
  do_check_eq(Identity.username, null);

  _("The 'username' attribute is normalized to lower case, updates preferences and identities.");
  Identity.username = "TarZan";
  do_check_eq(Identity.username, "tarzan");
  do_check_eq(Svc.Prefs.get("username"), "tarzan");
  do_check_eq(Identity.username, "tarzan");

  _("If not set, the 'account attribute' falls back to the username for backwards compatibility.");
  do_check_eq(Identity.account, "tarzan");

  _("Setting 'username' to a non-truthy value resets the pref.");
  Identity.username = null;
  do_check_eq(Identity.username, null);
  do_check_eq(Identity.account, null);
  const default_marker = {};
  do_check_eq(Svc.Prefs.get("username", default_marker), default_marker);
  do_check_eq(Identity.username, null);

  _("The 'account' attribute will set the 'username' if it doesn't contain characters that aren't allowed in the username.");
  Identity.account = "johndoe";
  do_check_eq(Identity.account, "johndoe");
  do_check_eq(Identity.username, "johndoe");
  do_check_eq(Svc.Prefs.get("username"), "johndoe");
  do_check_eq(Identity.username, "johndoe");

  _("If 'account' contains disallowed characters such as @, 'username' will the base32 encoded SHA1 hash of 'account'");
  Identity.account = "John@Doe.com";
  do_check_eq(Identity.account, "john@doe.com");
  do_check_eq(Identity.username, "7wohs32cngzuqt466q3ge7indszva4of");

  _("Setting 'account' to a non-truthy value resets the pref.");
  Identity.account = null;
  do_check_eq(Identity.account, null);
  do_check_eq(Svc.Prefs.get("account", default_marker), default_marker);
  do_check_eq(Identity.username, null);
  do_check_eq(Svc.Prefs.get("username", default_marker), default_marker);

  Svc.Prefs.resetBranch("");
  run_next_test();
});

add_test(function test_basic_password() {
  _("Ensure basic password setting works as expected.");

  Identity.account = null;
  do_check_eq(Identity.currentAuthState, LOGIN_FAILED_NO_USERNAME);
  let thrown = false;
  try {
    Identity.basicPassword = "foobar";
  } catch (ex) {
    thrown = true;
  }

  do_check_true(thrown);
  thrown = false;

  Identity.account = "johndoe";
  do_check_eq(Identity.currentAuthState, LOGIN_FAILED_NO_PASSWORD);
  Identity.basicPassword = "password";
  do_check_eq(Identity.basicPassword, "password");
  do_check_eq(Identity.currentAuthState, LOGIN_FAILED_NO_PASSPHRASE);
  do_check_true(Identity.hasBasicCredentials());

  Identity.account = null;

  run_next_test();
});

add_test(function test_basic_password_persistence() {
  _("Ensure credentials are saved and restored to the login manager properly.");

  // Just in case.
  Identity.account = null;
  Identity.deleteSyncCredentials();

  Identity.account = "janesmith";
  Identity.basicPassword = "ilovejohn";
  Identity.persistCredentials();

  let im1 = new IdentityManager();
  do_check_eq(im1._basicPassword, null);
  do_check_eq(im1.username, "janesmith");
  do_check_eq(im1.basicPassword, "ilovejohn");

  let im2 = new IdentityManager();
  do_check_eq(im2._basicPassword, null);

  _("Now remove the password and ensure it is deleted from storage.");
  Identity.basicPassword = null;
  Identity.persistCredentials(); // This should nuke from storage.
  do_check_eq(im2.basicPassword, null);

  _("Ensure that retrieving an unset but unpersisted removal returns null.");
  Identity.account = "janesmith";
  Identity.basicPassword = "myotherpassword";
  Identity.persistCredentials();

  Identity.basicPassword = null;
  do_check_eq(Identity.basicPassword, null);

  // Reset for next test.
  Identity.account = null;
  Identity.persistCredentials();

  run_next_test();
});

add_test(function test_sync_key() {
  _("Ensure Sync Key works as advertised.");

  _("Ensure setting a Sync Key before an account throws.");
  let thrown = false;
  try {
    Identity.syncKey = "blahblah";
  } catch (ex) {
    thrown = true;
  }
  do_check_true(thrown);
  thrown = false;

  Identity.account = "johnsmith";
  Identity.basicPassword = "johnsmithpw";

  do_check_eq(Identity.syncKey, null);
  do_check_eq(Identity.syncKeyBundle, null);

  _("An invalid Sync Key is silently accepted for historical reasons.");
  Identity.syncKey = "synckey";
  do_check_eq(Identity.syncKey, "synckey");

  _("But the SyncKeyBundle should not be created from bad keys.");
  do_check_eq(Identity.syncKeyBundle, null);

  let syncKey = Utils.generatePassphrase();
  Identity.syncKey = syncKey;
  do_check_eq(Identity.syncKey, syncKey);
  do_check_neq(Identity.syncKeyBundle, null);

  let im = new IdentityManager();
  im.account = "pseudojohn";
  do_check_eq(im.syncKey, null);
  do_check_eq(im.syncKeyBundle, null);

  Identity.account = null;

  run_next_test();
});

add_test(function test_sync_key_persistence() {
  _("Ensure Sync Key persistence works as expected.");

  Identity.account = "pseudojohn";
  Identity.password = "supersecret";

  let syncKey = Utils.generatePassphrase();
  Identity.syncKey = syncKey;

  Identity.persistCredentials();

  let im = new IdentityManager();
  im.account = "pseudojohn";
  do_check_eq(im.syncKey, syncKey);
  do_check_neq(im.syncKeyBundle, null);

  let kb1 = Identity.syncKeyBundle;
  let kb2 = im.syncKeyBundle;

  do_check_eq(kb1.encryptionKeyB64, kb2.encryptionKeyB64);
  do_check_eq(kb1.hmacKeyB64, kb2.hmacKeyB64);

  Identity.account = null;
  Identity.persistCredentials();

  let im2 = new IdentityManager();
  im2.account = "pseudojohn";
  do_check_eq(im2.syncKey, null);

  im2.account = null;

  _("Ensure deleted but not persisted value is retrieved.");
  Identity.account = "someoneelse";
  Identity.syncKey = Utils.generatePassphrase();
  Identity.persistCredentials();
  Identity.syncKey = null;
  do_check_eq(Identity.syncKey, null);

  // Clean up.
  Identity.account = null;
  Identity.persistCredentials();

  run_next_test();
});
