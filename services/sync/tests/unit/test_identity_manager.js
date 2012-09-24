/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");

let identity = new IdentityManager();

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Identity").level = Log4Moz.Level.Trace;

  run_next_test();
}

add_test(function test_username_from_account() {
  _("Ensure usernameFromAccount works properly.");

  do_check_eq(identity.usernameFromAccount(null), null);
  do_check_eq(identity.usernameFromAccount("user"), "user");
  do_check_eq(identity.usernameFromAccount("User"), "user");
  do_check_eq(identity.usernameFromAccount("john@doe.com"),
                                           "7wohs32cngzuqt466q3ge7indszva4of");

  run_next_test();
});

add_test(function test_account_username() {
  _("Ensure the account and username attributes work properly.");

  _("Verify initial state");
  do_check_eq(Svc.Prefs.get("account"), undefined);
  do_check_eq(Svc.Prefs.get("username"), undefined);
  do_check_eq(identity.account, null);
  do_check_eq(identity.username, null);

  _("The 'username' attribute is normalized to lower case, updates preferences and identities.");
  identity.username = "TarZan";
  do_check_eq(identity.username, "tarzan");
  do_check_eq(Svc.Prefs.get("username"), "tarzan");
  do_check_eq(identity.username, "tarzan");

  _("If not set, the 'account attribute' falls back to the username for backwards compatibility.");
  do_check_eq(identity.account, "tarzan");

  _("Setting 'username' to a non-truthy value resets the pref.");
  identity.username = null;
  do_check_eq(identity.username, null);
  do_check_eq(identity.account, null);
  const default_marker = {};
  do_check_eq(Svc.Prefs.get("username", default_marker), default_marker);
  do_check_eq(identity.username, null);

  _("The 'account' attribute will set the 'username' if it doesn't contain characters that aren't allowed in the username.");
  identity.account = "johndoe";
  do_check_eq(identity.account, "johndoe");
  do_check_eq(identity.username, "johndoe");
  do_check_eq(Svc.Prefs.get("username"), "johndoe");
  do_check_eq(identity.username, "johndoe");

  _("If 'account' contains disallowed characters such as @, 'username' will the base32 encoded SHA1 hash of 'account'");
  identity.account = "John@Doe.com";
  do_check_eq(identity.account, "john@doe.com");
  do_check_eq(identity.username, "7wohs32cngzuqt466q3ge7indszva4of");

  _("Setting 'account' to a non-truthy value resets the pref.");
  identity.account = null;
  do_check_eq(identity.account, null);
  do_check_eq(Svc.Prefs.get("account", default_marker), default_marker);
  do_check_eq(identity.username, null);
  do_check_eq(Svc.Prefs.get("username", default_marker), default_marker);

  Svc.Prefs.resetBranch("");
  run_next_test();
});

add_test(function test_basic_password() {
  _("Ensure basic password setting works as expected.");

  identity.account = null;
  do_check_eq(identity.currentAuthState, LOGIN_FAILED_NO_USERNAME);
  let thrown = false;
  try {
    identity.basicPassword = "foobar";
  } catch (ex) {
    thrown = true;
  }

  do_check_true(thrown);
  thrown = false;

  identity.account = "johndoe";
  do_check_eq(identity.currentAuthState, LOGIN_FAILED_NO_PASSWORD);
  identity.basicPassword = "password";
  do_check_eq(identity.basicPassword, "password");
  do_check_eq(identity.currentAuthState, LOGIN_FAILED_NO_PASSPHRASE);
  do_check_true(identity.hasBasicCredentials());

  identity.account = null;

  run_next_test();
});

add_test(function test_basic_password_persistence() {
  _("Ensure credentials are saved and restored to the login manager properly.");

  // Just in case.
  identity.account = null;
  identity.deleteSyncCredentials();

  identity.account = "janesmith";
  identity.basicPassword = "ilovejohn";
  identity.persistCredentials();

  let im1 = new IdentityManager();
  do_check_eq(im1._basicPassword, null);
  do_check_eq(im1.username, "janesmith");
  do_check_eq(im1.basicPassword, "ilovejohn");

  let im2 = new IdentityManager();
  do_check_eq(im2._basicPassword, null);

  _("Now remove the password and ensure it is deleted from storage.");
  identity.basicPassword = null;
  identity.persistCredentials(); // This should nuke from storage.
  do_check_eq(im2.basicPassword, null);

  _("Ensure that retrieving an unset but unpersisted removal returns null.");
  identity.account = "janesmith";
  identity.basicPassword = "myotherpassword";
  identity.persistCredentials();

  identity.basicPassword = null;
  do_check_eq(identity.basicPassword, null);

  // Reset for next test.
  identity.account = null;
  identity.persistCredentials();

  run_next_test();
});

add_test(function test_sync_key() {
  _("Ensure Sync Key works as advertised.");

  _("Ensure setting a Sync Key before an account throws.");
  let thrown = false;
  try {
    identity.syncKey = "blahblah";
  } catch (ex) {
    thrown = true;
  }
  do_check_true(thrown);
  thrown = false;

  identity.account = "johnsmith";
  identity.basicPassword = "johnsmithpw";

  do_check_eq(identity.syncKey, null);
  do_check_eq(identity.syncKeyBundle, null);

  _("An invalid Sync Key is silently accepted for historical reasons.");
  identity.syncKey = "synckey";
  do_check_eq(identity.syncKey, "synckey");

  _("But the SyncKeyBundle should not be created from bad keys.");
  do_check_eq(identity.syncKeyBundle, null);

  let syncKey = Utils.generatePassphrase();
  identity.syncKey = syncKey;
  do_check_eq(identity.syncKey, syncKey);
  do_check_neq(identity.syncKeyBundle, null);

  let im = new IdentityManager();
  im.account = "pseudojohn";
  do_check_eq(im.syncKey, null);
  do_check_eq(im.syncKeyBundle, null);

  identity.account = null;

  run_next_test();
});

add_test(function test_sync_key_changes() {
  _("Ensure changes to Sync Key have appropriate side-effects.");

  let im = new IdentityManager();
  let sk1 = Utils.generatePassphrase();
  let sk2 = Utils.generatePassphrase();

  im.account = "johndoe";
  do_check_eq(im.syncKey, null);
  do_check_eq(im.syncKeyBundle, null);

  im.syncKey = sk1;
  do_check_neq(im.syncKeyBundle, null);

  let ek1 = im.syncKeyBundle.encryptionKeyB64;
  let hk1 = im.syncKeyBundle.hmacKeyB64;

  // Change the Sync Key and ensure the Sync Key Bundle is updated.
  im.syncKey = sk2;
  let ek2 = im.syncKeyBundle.encryptionKeyB64;
  let hk2 = im.syncKeyBundle.hmacKeyB64;

  do_check_neq(ek1, ek2);
  do_check_neq(hk1, hk2);

  im.account = null;

  run_next_test();
});

add_test(function test_current_auth_state() {
  _("Ensure current auth state is reported properly.");

  let im = new IdentityManager();
  do_check_eq(im.currentAuthState, LOGIN_FAILED_NO_USERNAME);

  im.account = "johndoe";
  do_check_eq(im.currentAuthState, LOGIN_FAILED_NO_PASSWORD);

  im.basicPassword = "ilovejane";
  do_check_eq(im.currentAuthState, LOGIN_FAILED_NO_PASSPHRASE);

  im.syncKey = "foobar";
  do_check_eq(im.currentAuthState, LOGIN_FAILED_INVALID_PASSPHRASE);

  im.syncKey = null;
  do_check_eq(im.currentAuthState, LOGIN_FAILED_NO_PASSPHRASE);

  im.syncKey = Utils.generatePassphrase();
  do_check_eq(im.currentAuthState, STATUS_OK);

  im.account = null;

  run_next_test();
});

add_test(function test_sync_key_persistence() {
  _("Ensure Sync Key persistence works as expected.");

  identity.account = "pseudojohn";
  identity.password = "supersecret";

  let syncKey = Utils.generatePassphrase();
  identity.syncKey = syncKey;

  identity.persistCredentials();

  let im = new IdentityManager();
  im.account = "pseudojohn";
  do_check_eq(im.syncKey, syncKey);
  do_check_neq(im.syncKeyBundle, null);

  let kb1 = identity.syncKeyBundle;
  let kb2 = im.syncKeyBundle;

  do_check_eq(kb1.encryptionKeyB64, kb2.encryptionKeyB64);
  do_check_eq(kb1.hmacKeyB64, kb2.hmacKeyB64);

  identity.account = null;
  identity.persistCredentials();

  let im2 = new IdentityManager();
  im2.account = "pseudojohn";
  do_check_eq(im2.syncKey, null);

  im2.account = null;

  _("Ensure deleted but not persisted value is retrieved.");
  identity.account = "someoneelse";
  identity.syncKey = Utils.generatePassphrase();
  identity.persistCredentials();
  identity.syncKey = null;
  do_check_eq(identity.syncKey, null);

  // Clean up.
  identity.account = null;
  identity.persistCredentials();

  run_next_test();
});
