/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function login_handling(handler) {
  return function (request, response) {
    if (basic_auth_matches(request, "johndoe", "ilovejane") ||
        basic_auth_matches(request, "janedoe", "ilovejohn")) {
      handler(request, response);
    } else {
      let body = "Unauthorized";
      response.setStatusLine(request.httpVersion, 401, "Unauthorized");
      response.setHeader("Content-Type", "text/plain");
      response.bodyOutputStream.write(body, body.length);
    }
  };
}

function run_test() {
  let logger = Log.repository.rootLogger;
  Log.repository.rootLogger.addAppender(new Log.DumpAppender());

  run_next_test();
}

add_test(function test_offline() {
  try {
    _("The right bits are set when we're offline.");
    Services.io.offline = true;
    do_check_false(!!Service.login());
    do_check_eq(Service.status.login, LOGIN_FAILED_NETWORK_ERROR);
    Services.io.offline = false;
  } finally {
    Svc.Prefs.resetBranch("");
    run_next_test();
  }
});

function setup() {
  let janeHelper = track_collections_helper();
  let janeU      = janeHelper.with_updated_collection;
  let janeColls  = janeHelper.collections;
  let johnHelper = track_collections_helper();
  let johnU      = johnHelper.with_updated_collection;
  let johnColls  = johnHelper.collections;

  let server = httpd_setup({
    "/1.1/johndoe/info/collections": login_handling(johnHelper.handler),
    "/1.1/janedoe/info/collections": login_handling(janeHelper.handler),

    // We need these handlers because we test login, and login
    // is where keys are generated or fetched.
    // TODO: have Jane fetch her keys, not generate them...
    "/1.1/johndoe/storage/crypto/keys": johnU("crypto", new ServerWBO("keys").handler()),
    "/1.1/johndoe/storage/meta/global": johnU("meta",   new ServerWBO("global").handler()),
    "/1.1/janedoe/storage/crypto/keys": janeU("crypto", new ServerWBO("keys").handler()),
    "/1.1/janedoe/storage/meta/global": janeU("meta",   new ServerWBO("global").handler())
  });

  Service.serverURL = server.baseURI;
  return server;
}

add_test(function test_login_logout() {
  let server = setup();

  try {
    _("Force the initial state.");
    Service.status.service = STATUS_OK;
    do_check_eq(Service.status.service, STATUS_OK);

    _("Try logging in. It won't work because we're not configured yet.");
    Service.login();
    do_check_eq(Service.status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Service.status.login, LOGIN_FAILED_NO_USERNAME);
    do_check_false(Service.isLoggedIn);

    _("Try again with username and password set.");
    Service.identity.account = "johndoe";
    Service.identity.basicPassword = "ilovejane";
    Service.login();
    do_check_eq(Service.status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Service.status.login, LOGIN_FAILED_NO_PASSPHRASE);
    do_check_false(Service.isLoggedIn);

    _("Success if passphrase is set.");
    Service.identity.syncKey = "foo";
    Service.login();
    do_check_eq(Service.status.service, STATUS_OK);
    do_check_eq(Service.status.login, LOGIN_SUCCEEDED);
    do_check_true(Service.isLoggedIn);

    _("We can also pass username, password and passphrase to login().");
    Service.login("janedoe", "incorrectpassword", "bar");
    setBasicCredentials("janedoe", "incorrectpassword", "bar");
    do_check_eq(Service.status.service, LOGIN_FAILED);
    do_check_eq(Service.status.login, LOGIN_FAILED_LOGIN_REJECTED);
    do_check_false(Service.isLoggedIn);

    _("Try again with correct password.");
    Service.login("janedoe", "ilovejohn");
    do_check_eq(Service.status.service, STATUS_OK);
    do_check_eq(Service.status.login, LOGIN_SUCCEEDED);
    do_check_true(Service.isLoggedIn);

    _("Calling login() with parameters when the client is unconfigured sends notification.");
    let notified = false;
    Svc.Obs.add("weave:service:setup-complete", function() {
      notified = true;
    });
    setBasicCredentials(null, null, null);
    Service.login("janedoe", "ilovejohn", "bar");
    do_check_true(notified);
    do_check_eq(Service.status.service, STATUS_OK);
    do_check_eq(Service.status.login, LOGIN_SUCCEEDED);
    do_check_true(Service.isLoggedIn);

    _("Logout.");
    Service.logout();
    do_check_false(Service.isLoggedIn);

    _("Logging out again won't do any harm.");
    Service.logout();
    do_check_false(Service.isLoggedIn);

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(run_next_test);
  }
});

add_test(function test_login_on_sync() {
  let server = setup();
  setBasicCredentials("johndoe", "ilovejane", "bar");

  try {
    _("Sync calls login.");
    let oldLogin = Service.login;
    let loginCalled = false;
    Service.login = function() {
      loginCalled = true;
      Service.status.login = LOGIN_SUCCEEDED;
      this._loggedIn = false;           // So that sync aborts.
      return true;
    };

    Service.sync();

    do_check_true(loginCalled);
    Service.login = oldLogin;

    // Stub mpLocked.
    let mpLockedF = Utils.mpLocked;
    let mpLocked = true;
    Utils.mpLocked = function() mpLocked;

    // Stub scheduleNextSync. This gets called within checkSyncStatus if we're
    // ready to sync, so use it as an indicator.
    let scheduleNextSyncF = Service.scheduler.scheduleNextSync;
    let scheduleCalled = false;
    Service.scheduler.scheduleNextSync = function(wait) {
      scheduleCalled = true;
      scheduleNextSyncF.call(this, wait);
    };

    // Autoconnect still tries to connect in the background (useful behavior:
    // for non-MP users and unlocked MPs, this will detect version expiry
    // earlier).
    //
    // Consequently, non-MP users will be logged in as in the pre-Bug 543784 world,
    // and checkSyncStatus reflects that by waiting for login.
    //
    // This process doesn't apply if your MP is still locked, so we make
    // checkSyncStatus accept a locked MP in place of being logged in.
    //
    // This test exercises these two branches.

    _("We're ready to sync if locked.");
    Service.enabled = true;
    Services.io.offline = false;
    Service.scheduler.checkSyncStatus();
    do_check_true(scheduleCalled);

    _("... and also if we're not locked.");
    scheduleCalled = false;
    mpLocked = false;
    Service.scheduler.checkSyncStatus();
    do_check_true(scheduleCalled);
    Service.scheduler.scheduleNextSync = scheduleNextSyncF;

    // TODO: need better tests around master password prompting. See Bug 620583.

    mpLocked = true;

    // Testing exception handling if master password dialog is canceled.
    // Do this by monkeypatching.
    let oldGetter = Service.identity.__lookupGetter__("syncKey");
    let oldSetter = Service.identity.__lookupSetter__("syncKey");
    _("Old passphrase function is " + oldGetter);
    Service.identity.__defineGetter__("syncKey",
                           function() {
                             throw "User canceled Master Password entry";
                           });

    let oldClearSyncTriggers = Service.scheduler.clearSyncTriggers;
    let oldLockedSync = Service._lockedSync;

    let cSTCalled = false;
    let lockedSyncCalled = false;

    Service.scheduler.clearSyncTriggers = function() { cSTCalled = true; };
    Service._lockedSync = function() { lockedSyncCalled = true; };

    _("If master password is canceled, login fails and we report lockage.");
    do_check_false(!!Service.login());
    do_check_eq(Service.status.login, MASTER_PASSWORD_LOCKED);
    do_check_eq(Service.status.service, LOGIN_FAILED);
    _("Locked? " + Utils.mpLocked());
    _("checkSync reports the correct term.");
    do_check_eq(Service._checkSync(), kSyncMasterPasswordLocked);

    _("Sync doesn't proceed and clears triggers if MP is still locked.");
    Service.sync();

    do_check_true(cSTCalled);
    do_check_false(lockedSyncCalled);

    Service.identity.__defineGetter__("syncKey", oldGetter);
    Service.identity.__defineSetter__("syncKey", oldSetter);

    // N.B., a bunch of methods are stubbed at this point. Be careful putting
    // new tests after this point!

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(run_next_test);
  }
});
