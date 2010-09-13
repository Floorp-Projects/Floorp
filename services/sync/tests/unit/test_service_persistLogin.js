Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/constants.js");

function run_test() {
  try {
    Weave.Service.username = "johndoe";
    Weave.Service.password = "ilovejane";
    Weave.Service.passphrase = "my preciousss";

    _("Confirm initial environment is empty.");
    let logins = Weave.Svc.Login.findLogins({}, PWDMGR_HOST, null,
                                        PWDMGR_PASSWORD_REALM);
    do_check_eq(logins.length, 0);
    logins = Weave.Svc.Login.findLogins({}, PWDMGR_HOST, null,
                                        PWDMGR_PASSPHRASE_REALM);
    do_check_eq(logins.length, 0);

    _("Persist logins to the login service");
    Weave.Service.persistLogin();

    _("The password has been persisted in the login service.");
    logins = Weave.Svc.Login.findLogins({}, PWDMGR_HOST, null,
                                        PWDMGR_PASSWORD_REALM);
    do_check_eq(logins.length, 1);
    do_check_eq(logins[0].username, "johndoe");
    do_check_eq(logins[0].password, "ilovejane");

    _("The passphrase has been persisted in the login service.");
    logins = Weave.Svc.Login.findLogins({}, PWDMGR_HOST, null,
                                        PWDMGR_PASSPHRASE_REALM);
    do_check_eq(logins.length, 1);
    do_check_eq(logins[0].username, "johndoe");
    do_check_eq(logins[0].password, "my preciousss");

  } finally {
    Weave.Svc.Prefs.resetBranch("");
    Weave.Svc.Login.removeAllLogins();
  }
}
