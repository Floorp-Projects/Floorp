Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");

function login_handler(request, response) {
  // btoa('johndoe:ilovejane') == am9obmRvZTppbG92ZWphbmU=
  // btoa('janedoe:ilovejohn') == amFuZWRvZTppbG92ZWpvaG4=
  let body;
  let header = request.getHeader("Authorization");
  if (header == "Basic am9obmRvZTppbG92ZWphbmU="
      || header == "Basic amFuZWRvZTppbG92ZWpvaG4=") {
    body = "{}";
    response.setStatusLine(request.httpVersion, 200, "OK");
  } else {
    body = "Unauthorized";
    response.setStatusLine(request.httpVersion, 401, "Unauthorized");
  }
  response.bodyOutputStream.write(body, body.length);
}

function run_test() {
  let logger = Log4Moz.repository.rootLogger;
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  do_test_pending();
  let server = httpd_setup({
    "/1.0/johndoe/info/collections": login_handler,
    "/1.0/janedoe/info/collections": login_handler,
      
    // We need these handlers because we test login, and login
    // is where keys are generated or fetched.
    // TODO: have Jane fetch her keys, not generate them...
    "/1.0/johndoe/storage/crypto/keys": new ServerWBO().handler(),
    "/1.0/johndoe/storage/meta/global": new ServerWBO().handler(),
    "/1.0/janedoe/storage/crypto/keys": new ServerWBO().handler(),
    "/1.0/janedoe/storage/meta/global": new ServerWBO().handler()
  });

  try {
    Service.serverURL = "http://localhost:8080/";
    Service.clusterURL = "http://localhost:8080/";
    Svc.Prefs.set("autoconnect", false);

    _("Force the initial state.");
    Status.service = STATUS_OK;
    do_check_eq(Status.service, STATUS_OK);

    _("Try logging in. It won't work because we're not configured yet.");
    Service.login();
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);
    do_check_false(Service.isLoggedIn);
    do_check_false(Svc.Prefs.get("autoconnect"));

    _("Try again with username and password set.");
    Service.username = "johndoe";
    Service.password = "ilovejane";
    Service.login();
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSPHRASE);
    do_check_false(Service.isLoggedIn);
    do_check_false(Svc.Prefs.get("autoconnect"));

    _("Success if passphrase is set.");
    Service.passphrase = "foo";
    Service.login();
    do_check_eq(Status.service, STATUS_OK);
    do_check_eq(Status.login, LOGIN_SUCCEEDED);
    do_check_true(Service.isLoggedIn);
    do_check_true(Svc.Prefs.get("autoconnect"));

    _("We can also pass username, password and passphrase to login().");
    Service.login("janedoe", "incorrectpassword", "bar");
    do_check_eq(Service.username, "janedoe");
    do_check_eq(Service.password, "incorrectpassword");
    do_check_eq(Service.passphrase, "bar");
    do_check_eq(Status.service, LOGIN_FAILED);
    do_check_eq(Status.login, LOGIN_FAILED_LOGIN_REJECTED);
    do_check_false(Service.isLoggedIn);

    _("Try again with correct password.");
    Service.login("janedoe", "ilovejohn");
    do_check_eq(Status.service, STATUS_OK);
    do_check_eq(Status.login, LOGIN_SUCCEEDED);
    do_check_true(Service.isLoggedIn);
    do_check_true(Svc.Prefs.get("autoconnect"));
    
    _("Calling login() with parameters when the client is unconfigured sends notification.");
    let notified = false;
    Svc.Obs.add("weave:service:setup-complete", function() {
      notified = true;
    });
    Service.username = "";
    Service.password = "";
    Service.passphrase = "";    
    Service.login("janedoe", "ilovejohn", "bar");
    do_check_true(notified);
    do_check_eq(Status.service, STATUS_OK);
    do_check_eq(Status.login, LOGIN_SUCCEEDED);
    do_check_true(Service.isLoggedIn);
    do_check_true(Svc.Prefs.get("autoconnect"));

    _("Logout.");
    Service.logout();
    do_check_false(Service.isLoggedIn);
    do_check_false(Svc.Prefs.get("autoconnect"));

    _("Logging out again won't do any harm.");
    Service.logout();
    do_check_false(Service.isLoggedIn);
    do_check_false(Svc.Prefs.get("autoconnect"));
    
    /*
     * Testing login-on-sync.
     */
    
    _("Sync calls login.");
    let oldLogin = Service.login;
    let loginCalled = false;
    Service.login = function() {
      loginCalled = true;
      Status.login = LOGIN_SUCCEEDED;
      this._loggedIn = false;           // So that sync aborts.
      return true;
    }
    try {
      Service.sync();
    } catch (ex) {}
    
    do_check_true(loginCalled);
    Service.login = oldLogin;
    
    // Stub mpLocked.
    let mpLockedF = Utils.mpLocked;
    let mpLocked = true;
    Utils.mpLocked = function() mpLocked;
    
    // Stub scheduleNextSync. This gets called within checkSyncStatus if we're
    // ready to sync, so use it as an indicator.
    let scheduleNextSyncF = Service._scheduleNextSync;
    let scheduleCalled = false;
    Service._scheduleNextSync = function(wait) {
      scheduleCalled = true;
      scheduleNextSyncF.call(this, wait);
    }
    
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
    Svc.IO.offline = false;
    Service._checkSyncStatus();
    do_check_true(scheduleCalled);
    
    scheduleCalled = false;
    mpLocked = false;
    
    _("... and not if not.");
    Service._checkSyncStatus();
    do_check_false(scheduleCalled);
    Service._scheduleNextSync = scheduleNextSyncF;
    
    // TODO: need better tests around master password prompting. See Bug 620583.

    mpLocked = true;
    
    // Testing exception handling if master password dialog is canceled.
    // Do this by stubbing out Service.passphrase.
    let oldPP = Service.__lookupGetter__("passphrase");
    _("Old passphrase function is " + oldPP);
    Service.__defineGetter__("passphrase",
                           function() {
                             throw "User canceled Master Password entry";
                           });
    
    let oldClearSyncTriggers = Service._clearSyncTriggers;
    let oldLockedSync = Service._lockedSync;
    
    let cSTCalled = false;
    let lockedSyncCalled = false;
    
    Service._clearSyncTriggers = function() { cSTCalled = true; };
    Service._lockedSync = function() { lockedSyncCalled = true; };
    
    _("If master password is canceled, login fails and we report lockage.");
    do_check_false(!!Service.login());
    do_check_eq(Status.login, MASTER_PASSWORD_LOCKED);
    do_check_eq(Status.service, LOGIN_FAILED);
    _("Locked? " + Utils.mpLocked());
    _("checkSync reports the correct term.");
    do_check_eq(Service._checkSync(), kSyncMasterPasswordLocked);
    
    _("Sync doesn't proceed and clears triggers if MP is still locked.");
    Service.sync();
    
    do_check_true(cSTCalled);
    do_check_false(lockedSyncCalled);

    // N.B., a bunch of methods are stubbed at this point. Be careful putting
    // new tests after this point!
    
  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
}
