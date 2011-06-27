Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");

function test_identities() {
  _("Account related Service properties correspond to preference settings and update other object properties upon being set.");

  try {
    _("Verify initial state");
    do_check_eq(Svc.Prefs.get("account"), undefined);
    do_check_eq(Svc.Prefs.get("username"), undefined);
    do_check_eq(ID.get("WeaveID").username, "");
    do_check_eq(ID.get("WeaveCryptoID").username, "");

    _("The 'username' attribute is normalized to lower case, updates preferences and identities.");
    Service.username = "TarZan";
    do_check_eq(Service.username, "tarzan");
    do_check_eq(Svc.Prefs.get("username"), "tarzan");
    do_check_eq(ID.get("WeaveID").username, "tarzan");
    do_check_eq(ID.get("WeaveCryptoID").username, "tarzan");

    _("If not set, the 'account attribute' falls back to the username for backwards compatibility.");
    do_check_eq(Service.account, "tarzan");

    _("Setting 'username' to a non-truthy value resets the pref.");
    Service.username = null;
    do_check_eq(Service.username, "");
    do_check_eq(Service.account, "");
    const default_marker = {};
    do_check_eq(Svc.Prefs.get("username", default_marker), default_marker);
    do_check_eq(ID.get("WeaveID").username, null);
    do_check_eq(ID.get("WeaveCryptoID").username, null);

    _("The 'account' attribute will set the 'username' if it doesn't contain characters that aren't allowed in the username.");
    Service.account = "johndoe";
    do_check_eq(Service.account, "johndoe");
    do_check_eq(Service.username, "johndoe");
    do_check_eq(Svc.Prefs.get("username"), "johndoe");
    do_check_eq(ID.get("WeaveID").username, "johndoe");
    do_check_eq(ID.get("WeaveCryptoID").username, "johndoe");

    _("If 'account' contains disallowed characters such as @, 'username' will the base32 encoded SHA1 hash of 'account'");
    Service.account = "John@Doe.com";
    do_check_eq(Service.account, "john@doe.com");
    do_check_eq(Service.username, "7wohs32cngzuqt466q3ge7indszva4of");

    _("Setting 'account' to a non-truthy value resets the pref.");
    Service.account = null;
    do_check_eq(Service.account, "");
    do_check_eq(Svc.Prefs.get("account", default_marker), default_marker);
    do_check_eq(Service.username, "");
    do_check_eq(Svc.Prefs.get("username", default_marker), default_marker);

  } finally {
    Svc.Prefs.resetBranch("");
  }
}

function test_urls() {
  _("URL related Service properties corresopnd to preference settings.");
  try {
    do_check_true(!!Service.serverURL); // actual value may change
    do_check_eq(Service.clusterURL, "");
    do_check_eq(Service.userBaseURL, undefined);
    do_check_eq(Service.infoURL, undefined);
    do_check_eq(Service.storageURL, undefined);
    do_check_eq(Service.metaURL, undefined);

    _("The 'clusterURL' attribute updates preferences and cached URLs.");
    Service.username = "johndoe";

    // Since we don't have a cluster URL yet, these will still not be defined.
    do_check_eq(Service.infoURL, undefined);
    do_check_eq(Service.userBaseURL, undefined);
    do_check_eq(Service.storageURL, undefined);
    do_check_eq(Service.metaURL, undefined);

    Service.serverURL = "http://weave.server/";
    Service.clusterURL = "http://weave.cluster/";
    do_check_eq(Svc.Prefs.get("clusterURL"), "http://weave.cluster/");

    do_check_eq(Service.userBaseURL, "http://weave.cluster/1.1/johndoe/");
    do_check_eq(Service.infoURL,
                "http://weave.cluster/1.1/johndoe/info/collections");
    do_check_eq(Service.storageURL,
                "http://weave.cluster/1.1/johndoe/storage/");
    do_check_eq(Service.metaURL,
                "http://weave.cluster/1.1/johndoe/storage/meta/global");

    _("The 'miscURL' and 'userURL' attributes can be relative to 'serverURL' or absolute.");
    Svc.Prefs.set("miscURL", "relative/misc/");
    Svc.Prefs.set("userURL", "relative/user/");
    do_check_eq(Service.miscAPI,
                "http://weave.server/relative/misc/1.0/");
    do_check_eq(Service.userAPI,
                "http://weave.server/relative/user/1.0/");

    Svc.Prefs.set("miscURL", "http://weave.misc.services/");
    Svc.Prefs.set("userURL", "http://weave.user.services/");
    do_check_eq(Service.miscAPI, "http://weave.misc.services/1.0/");
    do_check_eq(Service.userAPI, "http://weave.user.services/1.0/");

    do_check_eq(Service.pwResetURL,
                "http://weave.server/weave-password-reset");

    _("Empty/false value for 'username' resets preference.");
    Service.username = "";
    do_check_eq(Svc.Prefs.get("username"), undefined);
    do_check_eq(ID.get("WeaveID").username, "");
    do_check_eq(ID.get("WeaveCryptoID").username, "");

    _("The 'serverURL' attributes updates/resets preferences.");
    // Identical value doesn't do anything
    Service.serverURL = Service.serverURL;
    do_check_eq(Svc.Prefs.get("clusterURL"), "http://weave.cluster/");

    Service.serverURL = "http://different.auth.node/";
    do_check_eq(Svc.Prefs.get("serverURL"), "http://different.auth.node/");
    do_check_eq(Svc.Prefs.get("clusterURL"), undefined);

  } finally {
    Svc.Prefs.resetBranch("");
  }
}


function test_syncID() {
  _("Service.syncID is auto-generated, corresponds to preference.");
  new FakeGUIDService();

  try {
    // Ensure pristine environment
    do_check_eq(Svc.Prefs.get("client.syncID"), undefined);

    // Performing the first get on the attribute will generate a new GUID.
    do_check_eq(Service.syncID, "fake-guid-0");
    do_check_eq(Svc.Prefs.get("client.syncID"), "fake-guid-0");

    Svc.Prefs.set("client.syncID", Utils.makeGUID());
    do_check_eq(Svc.Prefs.get("client.syncID"), "fake-guid-1");
    do_check_eq(Service.syncID, "fake-guid-1");
  } finally {
    Svc.Prefs.resetBranch("");
    new FakeGUIDService();
  }
}

function test_locked() {
  _("The 'locked' attribute can be toggled with lock() and unlock()");

  // Defaults to false
  do_check_eq(Service.locked, false);

  do_check_eq(Service.lock(), true);
  do_check_eq(Service.locked, true);

  // Locking again will return false
  do_check_eq(Service.lock(), false);

  Service.unlock();
  do_check_eq(Service.locked, false);
}

function run_test() {
  test_identities();
  test_urls();
  test_syncID();
  test_locked();
}
