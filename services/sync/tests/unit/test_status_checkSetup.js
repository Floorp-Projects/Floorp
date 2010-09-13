Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  try {
    _("Verify initial setup.");
    do_check_eq(ID.get("WeaveID"), null);
    do_check_eq(ID.get("WeaveCryptoID"), null);

    _("Fresh setup, we're not configured.");
    do_check_eq(Status.checkSetup(), CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);
    Status.resetSync();

    _("Let's provide a username.");
    Svc.Prefs.set("username", "johndoe");
    do_check_eq(Status.checkSetup(), CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSWORD);
    Status.resetSync();

    _("checkSetup() created a WeaveID identity.");
    let id = ID.get("WeaveID");
    do_check_true(!!id);

    _("Let's provide a password.");
    id.password = "carotsalad";
    do_check_eq(Status.checkSetup(), CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSPHRASE);
    Status.resetSync();

    _("checkSetup() created a WeaveCryptoID identity");
    id = ID.get("WeaveCryptoID");
    do_check_true(!!id);

    _("Let's provide a passphrase");
    id.password = "chickeninacan";
    do_check_eq(Status.checkSetup(), STATUS_OK);
    Status.resetSync();

  } finally {
    Svc.Prefs.resetBranch("");
  }
}
