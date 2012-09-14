/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  initTestLogging("Trace");

  try {
    _("Ensure fresh config.");
    Status._authManager.deleteSyncCredentials();

    _("Fresh setup, we're not configured.");
    do_check_eq(Status.checkSetup(), CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);
    Status.resetSync();

    _("Let's provide a username.");
    Status._authManager.username = "johndoe";
    do_check_eq(Status.checkSetup(), CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSWORD);
    Status.resetSync();

    do_check_neq(Status._authManager.username, null);

    _("Let's provide a password.");
    Status._authManager.basicPassword = "carotsalad";
    do_check_eq(Status.checkSetup(), CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSPHRASE);
    Status.resetSync();

    _("Let's provide a passphrase");
    Status._authManager.syncKey = "a-bcdef-abcde-acbde-acbde-acbde";
    _("checkSetup()");
    do_check_eq(Status.checkSetup(), STATUS_OK);
    Status.resetSync();

  } finally {
    Svc.Prefs.resetBranch("");
  }
}
