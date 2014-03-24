/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function run_test() {
  try {
    // Ensure we have a blank slate to start.
    ensureLegacyIdentityManager();
    Services.logins.removeAllLogins();

    setBasicCredentials("johndoe", "ilovejane", "abbbbbcccccdddddeeeeefffff");

    _("Confirm initial environment is empty.");
    let logins = Services.logins.findLogins({}, PWDMGR_HOST, null,
                                        PWDMGR_PASSWORD_REALM);
    do_check_eq(logins.length, 0);
    logins = Services.logins.findLogins({}, PWDMGR_HOST, null,
                                        PWDMGR_PASSPHRASE_REALM);
    do_check_eq(logins.length, 0);

    _("Persist logins to the login service");
    Service.persistLogin();

    _("The password has been persisted in the login service.");
    logins = Services.logins.findLogins({}, PWDMGR_HOST, null,
                                        PWDMGR_PASSWORD_REALM);
    do_check_eq(logins.length, 1);
    do_check_eq(logins[0].username, "johndoe");
    do_check_eq(logins[0].password, "ilovejane");

    _("The passphrase has been persisted in the login service.");
    logins = Services.logins.findLogins({}, PWDMGR_HOST, null,
                                        PWDMGR_PASSPHRASE_REALM);
    do_check_eq(logins.length, 1);
    do_check_eq(logins[0].username, "johndoe");
    do_check_eq(logins[0].password, "abbbbbcccccdddddeeeeefffff");

  } finally {
    Svc.Prefs.resetBranch("");
    Services.logins.removeAllLogins();
  }
}
