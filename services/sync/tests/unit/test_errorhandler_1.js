/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://gre/modules/FileUtils.jsm");

var fakeServer = new SyncServer();
fakeServer.start();

do_register_cleanup(function() {
  return new Promise(resolve => {
    fakeServer.stop(resolve);
  });
});

var fakeServerUrl = "http://localhost:" + fakeServer.port;

const logsdir = FileUtils.getDir("ProfD", ["weave", "logs"], true);

const PROLONGED_ERROR_DURATION =
  (Svc.Prefs.get('errorhandler.networkFailureReportTimeout') * 2) * 1000;

const NON_PROLONGED_ERROR_DURATION =
  (Svc.Prefs.get('errorhandler.networkFailureReportTimeout') / 2) * 1000;

Service.engineManager.clear();

function setLastSync(lastSyncValue) {
  Svc.Prefs.set("lastSync", (new Date(Date.now() - lastSyncValue)).toString());
}

var engineManager = Service.engineManager;
engineManager.register(EHTestsCommon.CatapultEngine);

// This relies on Service/ErrorHandler being a singleton. Fixing this will take
// a lot of work.
var errorHandler = Service.errorHandler;

function run_test() {
  initTestLogging("Trace");

  Log.repository.getLogger("Sync.Service").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.SyncScheduler").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.ErrorHandler").level = Log.Level.Trace;

  ensureLegacyIdentityManager();

  run_next_test();
}


function clean() {
  Service.startOver();
  Status.resetSync();
  Status.resetBackoff();
  errorHandler.didReportProlongedError = false;
}

add_identity_test(this, function* test_401_logout() {
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  // By calling sync, we ensure we're logged in.
  yield sync_and_validate_telem();
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_true(Service.isLoggedIn);

  let deferred = Promise.defer();
  Svc.Obs.add("weave:service:sync:error", onSyncError);
  function onSyncError() {
    _("Got weave:service:sync:error in first sync.");
    Svc.Obs.remove("weave:service:sync:error", onSyncError);

    // Wait for the automatic next sync.
    function onLoginError() {
      _("Got weave:service:login:error in second sync.");
      Svc.Obs.remove("weave:service:login:error", onLoginError);

      let expected = isConfiguredWithLegacyIdentity() ?
                     LOGIN_FAILED_LOGIN_REJECTED : LOGIN_FAILED_NETWORK_ERROR;

      do_check_eq(Status.login, expected);
      do_check_false(Service.isLoggedIn);

      // Clean up.
      Utils.nextTick(function () {
        Service.startOver();
        server.stop(deferred.resolve);
      });
    }
    Svc.Obs.add("weave:service:login:error", onLoginError);
  }

  // Make sync fail due to login rejected.
  yield configureIdentity({username: "janedoe"});
  Service._updateCachedURLs();

  _("Starting first sync.");
  let ping = yield sync_and_validate_telem(true);
  deepEqual(ping.failureReason, { name: "httperror", code: 401 });
  _("First sync done.");
  yield deferred.promise;
});

add_identity_test(this, function* test_credentials_changed_logout() {
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  // By calling sync, we ensure we're logged in.
  yield sync_and_validate_telem();
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_true(Service.isLoggedIn);

  EHTestsCommon.generateCredentialsChangedFailure();

  let ping = yield sync_and_validate_telem(true);
  equal(ping.status.sync, CREDENTIALS_CHANGED);
  deepEqual(ping.failureReason, {
    name: "unexpectederror",
    error: "Error: Aborting sync, remote setup failed"
  });

  do_check_eq(Status.sync, CREDENTIALS_CHANGED);
  do_check_false(Service.isLoggedIn);

  // Clean up.
  Service.startOver();
  let deferred = Promise.defer();
  server.stop(deferred.resolve);
  yield deferred.promise;
});

add_identity_test(this, function test_no_lastSync_pref() {
  // Test reported error.
  Status.resetSync();
  errorHandler.dontIgnoreErrors = true;
  Status.sync = CREDENTIALS_CHANGED;
  do_check_true(errorHandler.shouldReportError());

  // Test unreported error.
  Status.resetSync();
  errorHandler.dontIgnoreErrors = true;
  Status.login = LOGIN_FAILED_NETWORK_ERROR;
  do_check_true(errorHandler.shouldReportError());

});

add_identity_test(this, function test_shouldReportError() {
  Status.login = MASTER_PASSWORD_LOCKED;
  do_check_false(errorHandler.shouldReportError());

  // Give ourselves a clusterURL so that the temporary 401 no-error situation
  // doesn't come into play.
  Service.serverURL  = fakeServerUrl;
  Service.clusterURL = fakeServerUrl;

  // Test dontIgnoreErrors, non-network, non-prolonged, login error reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.login = LOGIN_FAILED_NO_PASSWORD;
  do_check_true(errorHandler.shouldReportError());

  // Test dontIgnoreErrors, non-network, non-prolonged, sync error reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.sync = CREDENTIALS_CHANGED;
  do_check_true(errorHandler.shouldReportError());

  // Test dontIgnoreErrors, non-network, prolonged, login error reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.login = LOGIN_FAILED_NO_PASSWORD;
  do_check_true(errorHandler.shouldReportError());

  // Test dontIgnoreErrors, non-network, prolonged, sync error reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.sync = CREDENTIALS_CHANGED;
  do_check_true(errorHandler.shouldReportError());

  // Test dontIgnoreErrors, network, non-prolonged, login error reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.login = LOGIN_FAILED_NETWORK_ERROR;
  do_check_true(errorHandler.shouldReportError());

  // Test dontIgnoreErrors, network, non-prolonged, sync error reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.sync = LOGIN_FAILED_NETWORK_ERROR;
  do_check_true(errorHandler.shouldReportError());

  // Test dontIgnoreErrors, network, prolonged, login error reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.login = LOGIN_FAILED_NETWORK_ERROR;
  do_check_true(errorHandler.shouldReportError());

  // Test dontIgnoreErrors, network, prolonged, sync error reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.sync = LOGIN_FAILED_NETWORK_ERROR;
  do_check_true(errorHandler.shouldReportError());

  // Test non-network, prolonged, login error reported
  do_check_false(errorHandler.didReportProlongedError);
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.login = LOGIN_FAILED_NO_PASSWORD;
  do_check_true(errorHandler.shouldReportError());
  do_check_true(errorHandler.didReportProlongedError);

  // Second time with prolonged error and without resetting
  // didReportProlongedError, sync error should not be reported.
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.login = LOGIN_FAILED_NO_PASSWORD;
  do_check_false(errorHandler.shouldReportError());
  do_check_true(errorHandler.didReportProlongedError);

  // Test non-network, prolonged, sync error reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  errorHandler.didReportProlongedError = false;
  Status.sync = CREDENTIALS_CHANGED;
  do_check_true(errorHandler.shouldReportError());
  do_check_true(errorHandler.didReportProlongedError);
  errorHandler.didReportProlongedError = false;

  // Test network, prolonged, login error reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.login = LOGIN_FAILED_NETWORK_ERROR;
  do_check_true(errorHandler.shouldReportError());
  do_check_true(errorHandler.didReportProlongedError);
  errorHandler.didReportProlongedError = false;

  // Test network, prolonged, sync error reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.sync = LOGIN_FAILED_NETWORK_ERROR;
  do_check_true(errorHandler.shouldReportError());
  do_check_true(errorHandler.didReportProlongedError);
  errorHandler.didReportProlongedError = false;

  // Test non-network, non-prolonged, login error reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.login = LOGIN_FAILED_NO_PASSWORD;
  do_check_true(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);

  // Test non-network, non-prolonged, sync error reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.sync = CREDENTIALS_CHANGED;
  do_check_true(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);

  // Test network, non-prolonged, login error reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.login = LOGIN_FAILED_NETWORK_ERROR;
  do_check_false(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);

  // Test network, non-prolonged, sync error reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.sync = LOGIN_FAILED_NETWORK_ERROR;
  do_check_false(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);

  // Test server maintenance, sync errors are not reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.sync = SERVER_MAINTENANCE;
  do_check_false(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);

  // Test server maintenance, login errors are not reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.login = SERVER_MAINTENANCE;
  do_check_false(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);

  // Test prolonged, server maintenance, sync errors are reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.sync = SERVER_MAINTENANCE;
  do_check_true(errorHandler.shouldReportError());
  do_check_true(errorHandler.didReportProlongedError);
  errorHandler.didReportProlongedError = false;

  // Test prolonged, server maintenance, login errors are reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = false;
  Status.login = SERVER_MAINTENANCE;
  do_check_true(errorHandler.shouldReportError());
  do_check_true(errorHandler.didReportProlongedError);
  errorHandler.didReportProlongedError = false;

  // Test dontIgnoreErrors, server maintenance, sync errors are reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.sync = SERVER_MAINTENANCE;
  do_check_true(errorHandler.shouldReportError());
  // dontIgnoreErrors means we don't set didReportProlongedError
  do_check_false(errorHandler.didReportProlongedError);

  // Test dontIgnoreErrors, server maintenance, login errors are reported
  Status.resetSync();
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.login = SERVER_MAINTENANCE;
  do_check_true(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);

  // Test dontIgnoreErrors, prolonged, server maintenance,
  // sync errors are reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.sync = SERVER_MAINTENANCE;
  do_check_true(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);

  // Test dontIgnoreErrors, prolonged, server maintenance,
  // login errors are reported
  Status.resetSync();
  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.dontIgnoreErrors = true;
  Status.login = SERVER_MAINTENANCE;
  do_check_true(errorHandler.shouldReportError());
  do_check_false(errorHandler.didReportProlongedError);
});

add_identity_test(this, function* test_shouldReportError_master_password() {
  _("Test error ignored due to locked master password");
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  // Monkey patch Service.verifyLogin to imitate
  // master password being locked.
  Service._verifyLogin = Service.verifyLogin;
  Service.verifyLogin = function () {
    Status.login = MASTER_PASSWORD_LOCKED;
    return false;
  };

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  Service.sync();
  do_check_false(errorHandler.shouldReportError());

  // Clean up.
  Service.verifyLogin = Service._verifyLogin;
  clean();
  let deferred = Promise.defer();
  server.stop(deferred.resolve);
  yield deferred.promise;
});

// Test that even if we don't have a cluster URL, a login failure due to
// authentication errors is always reported.
add_identity_test(this, function test_shouldReportLoginFailureWithNoCluster() {
  // Ensure no clusterURL - any error not specific to login should not be reported.
  Service.serverURL  = "";
  Service.clusterURL = "";

  // Test explicit "login rejected" state.
  Status.resetSync();
  // If we have a LOGIN_REJECTED state, we always report the error.
  Status.login = LOGIN_FAILED_LOGIN_REJECTED;
  do_check_true(errorHandler.shouldReportError());
  // But any other status with a missing clusterURL is treated as a mid-sync
  // 401 (ie, should be treated as a node reassignment)
  Status.login = LOGIN_SUCCEEDED;
  do_check_false(errorHandler.shouldReportError());
});

// XXX - how to arrange for 'Service.identity.basicPassword = null;' in
// an fxaccounts environment?
add_task(function* test_login_syncAndReportErrors_non_network_error() {
  // Test non-network errors are reported
  // when calling syncAndReportErrors
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);
  Service.identity.basicPassword = null;

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:login:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:login:error", onSyncError);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSWORD);

    clean();
    server.stop(deferred.resolve);
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  yield deferred.promise;
});

add_identity_test(this, function* test_sync_syncAndReportErrors_non_network_error() {
  // Test non-network errors are reported
  // when calling syncAndReportErrors
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  // By calling sync, we ensure we're logged in.
  Service.sync();
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_true(Service.isLoggedIn);

  EHTestsCommon.generateCredentialsChangedFailure();

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:sync:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:sync:error", onSyncError);
    do_check_eq(Status.sync, CREDENTIALS_CHANGED);
    // If we clean this tick, telemetry won't get the right error
    server.stop(() => {
      clean();
      deferred.resolve();
    });
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  let ping = yield wait_for_ping(() => errorHandler.syncAndReportErrors(), true);
  equal(ping.status.sync, CREDENTIALS_CHANGED);
  deepEqual(ping.failureReason, {
    name: "unexpectederror",
    error: "Error: Aborting sync, remote setup failed"
  });
  yield deferred.promise;
});

// XXX - how to arrange for 'Service.identity.basicPassword = null;' in
// an fxaccounts environment?
add_task(function* test_login_syncAndReportErrors_prolonged_non_network_error() {
  // Test prolonged, non-network errors are
  // reported when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);
  Service.identity.basicPassword = null;

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:login:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:login:error", onSyncError);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSWORD);

    clean();
    server.stop(deferred.resolve);
  });

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  yield deferred.promise;
});

add_identity_test(this, function* test_sync_syncAndReportErrors_prolonged_non_network_error() {
  // Test prolonged, non-network errors are
  // reported when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  // By calling sync, we ensure we're logged in.
  Service.sync();
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_true(Service.isLoggedIn);

  EHTestsCommon.generateCredentialsChangedFailure();

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:sync:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:sync:error", onSyncError);
    do_check_eq(Status.sync, CREDENTIALS_CHANGED);
    // If we clean this tick, telemetry won't get the right error
    server.stop(() => {
      clean();
      deferred.resolve();
    });
  });

  setLastSync(PROLONGED_ERROR_DURATION);
  let ping = yield wait_for_ping(() => errorHandler.syncAndReportErrors(), true);
  equal(ping.status.sync, CREDENTIALS_CHANGED);
  deepEqual(ping.failureReason, {
    name: "unexpectederror",
    error: "Error: Aborting sync, remote setup failed"
  });
  yield deferred.promise;
});

add_identity_test(this, function* test_login_syncAndReportErrors_network_error() {
  // Test network errors are reported when calling syncAndReportErrors.
  yield configureIdentity({username: "broken.wipe"});
  Service.serverURL  = fakeServerUrl;
  Service.clusterURL = fakeServerUrl;

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:login:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:login:error", onSyncError);
    do_check_eq(Status.login, LOGIN_FAILED_NETWORK_ERROR);

    clean();
    deferred.resolve();
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  yield deferred.promise;
});


add_test(function test_sync_syncAndReportErrors_network_error() {
  // Test network errors are reported when calling syncAndReportErrors.
  Services.io.offline = true;

  Svc.Obs.add("weave:ui:sync:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:sync:error", onSyncError);
    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);

    Services.io.offline = false;
    clean();
    run_next_test();
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
});

add_identity_test(this, function* test_login_syncAndReportErrors_prolonged_network_error() {
  // Test prolonged, network errors are reported
  // when calling syncAndReportErrors.
  yield configureIdentity({username: "johndoe"});

  Service.serverURL  = fakeServerUrl;
  Service.clusterURL = fakeServerUrl;

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:login:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:login:error", onSyncError);
    do_check_eq(Status.login, LOGIN_FAILED_NETWORK_ERROR);

    clean();
    deferred.resolve();
  });

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  yield deferred.promise;
});

add_test(function test_sync_syncAndReportErrors_prolonged_network_error() {
  // Test prolonged, network errors are reported
  // when calling syncAndReportErrors.
  Services.io.offline = true;

  Svc.Obs.add("weave:ui:sync:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:sync:error", onSyncError);
    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);

    Services.io.offline = false;
    clean();
    run_next_test();
  });

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
});

add_task(function* test_login_prolonged_non_network_error() {
  // Test prolonged, non-network errors are reported
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);
  Service.identity.basicPassword = null;

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:login:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:login:error", onSyncError);
    do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
    do_check_true(errorHandler.didReportProlongedError);

    clean();
    server.stop(deferred.resolve);
  });

  setLastSync(PROLONGED_ERROR_DURATION);
  Service.sync();
  yield deferred.promise;
});

add_task(function* test_sync_prolonged_non_network_error() {
  // Test prolonged, non-network errors are reported
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  // By calling sync, we ensure we're logged in.
  Service.sync();
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_true(Service.isLoggedIn);

  EHTestsCommon.generateCredentialsChangedFailure();

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:sync:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:sync:error", onSyncError);
    do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
    do_check_true(errorHandler.didReportProlongedError);
    server.stop(() => {
      clean();
      deferred.resolve();
    });
  });

  setLastSync(PROLONGED_ERROR_DURATION);

  let ping = yield sync_and_validate_telem(true);
  equal(ping.status.sync, PROLONGED_SYNC_FAILURE);
  deepEqual(ping.failureReason, {
    name: "unexpectederror",
    error: "Error: Aborting sync, remote setup failed"
  });
  yield deferred.promise;
});

add_identity_test(this, function* test_login_prolonged_network_error() {
  // Test prolonged, network errors are reported
  yield configureIdentity({username: "johndoe"});
  Service.serverURL  = fakeServerUrl;
  Service.clusterURL = fakeServerUrl;

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:login:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:login:error", onSyncError);
    do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
    do_check_true(errorHandler.didReportProlongedError);

    clean();
    deferred.resolve();
  });

  setLastSync(PROLONGED_ERROR_DURATION);
  Service.sync();
  yield deferred.promise;
});

add_test(function test_sync_prolonged_network_error() {
  // Test prolonged, network errors are reported
  Services.io.offline = true;

  Svc.Obs.add("weave:ui:sync:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:sync:error", onSyncError);
    do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
    do_check_true(errorHandler.didReportProlongedError);

    Services.io.offline = false;
    clean();
    run_next_test();
  });

  setLastSync(PROLONGED_ERROR_DURATION);
  Service.sync();
});

add_task(function* test_login_non_network_error() {
  // Test non-network errors are reported
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);
  Service.identity.basicPassword = null;

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:login:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:login:error", onSyncError);
    do_check_eq(Status.login, LOGIN_FAILED_NO_PASSWORD);
    do_check_false(errorHandler.didReportProlongedError);

    clean();
    server.stop(deferred.resolve);
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  Service.sync();
  yield deferred.promise;
});

add_task(function* test_sync_non_network_error() {
  // Test non-network errors are reported
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  // By calling sync, we ensure we're logged in.
  Service.sync();
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_true(Service.isLoggedIn);

  EHTestsCommon.generateCredentialsChangedFailure();

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:sync:error", function onSyncError() {
    Svc.Obs.remove("weave:ui:sync:error", onSyncError);
    do_check_eq(Status.sync, CREDENTIALS_CHANGED);
    do_check_false(errorHandler.didReportProlongedError);

    clean();
    server.stop(deferred.resolve);
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  Service.sync();
  yield deferred.promise;
});

add_identity_test(this, function* test_login_network_error() {
  yield configureIdentity({username: "johndoe"});
  Service.serverURL  = fakeServerUrl;
  Service.clusterURL = fakeServerUrl;

  let deferred = Promise.defer();
  // Test network errors are not reported.
  Svc.Obs.add("weave:ui:clear-error", function onClearError() {
    Svc.Obs.remove("weave:ui:clear-error", onClearError);

    do_check_eq(Status.login, LOGIN_FAILED_NETWORK_ERROR);
    do_check_false(errorHandler.didReportProlongedError);

    Services.io.offline = false;
    clean();
    deferred.resolve()
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  Service.sync();
  yield deferred.promise;
});

add_test(function test_sync_network_error() {
  // Test network errors are not reported.
  Services.io.offline = true;

  Svc.Obs.add("weave:ui:sync:finish", function onUIUpdate() {
    Svc.Obs.remove("weave:ui:sync:finish", onUIUpdate);
    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
    do_check_false(errorHandler.didReportProlongedError);

    Services.io.offline = false;
    clean();
    run_next_test();
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  Service.sync();
});

add_identity_test(this, function* test_sync_server_maintenance_error() {
  // Test server maintenance errors are not reported.
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  const BACKOFF = 42;
  let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 503,
                      headers: {"retry-after": BACKOFF}};

  function onSyncError() {
    do_throw("Shouldn't get here!");
  }
  Svc.Obs.add("weave:ui:sync:error", onSyncError);

  do_check_eq(Status.service, STATUS_OK);

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:ui:sync:finish", onSyncFinish);

    do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
    do_check_eq(Status.sync, SERVER_MAINTENANCE);
    do_check_false(errorHandler.didReportProlongedError);

    Svc.Obs.remove("weave:ui:sync:error", onSyncError);
    server.stop(() => {
      clean();
      deferred.resolve();
    })
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  let ping = yield sync_and_validate_telem(true);
  equal(ping.status.sync, SERVER_MAINTENANCE);
  deepEqual(ping.engines.find(e => e.failureReason).failureReason, { name: "httperror", code: 503 })

  yield deferred.promise;
});

add_identity_test(this, function* test_info_collections_login_server_maintenance_error() {
  // Test info/collections server maintenance errors are not reported.
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  Service.username = "broken.info";
  yield configureIdentity({username: "broken.info"});
  Service.serverURL = server.baseURI + "/maintenance/";
  Service.clusterURL = server.baseURI + "/maintenance/";

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  function onUIUpdate() {
    do_throw("Shouldn't experience UI update!");
  }
  Svc.Obs.add("weave:ui:login:error", onUIUpdate);

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:clear-error", function onLoginFinish() {
    Svc.Obs.remove("weave:ui:clear-error", onLoginFinish);

    do_check_true(Status.enforceBackoff);
    do_check_eq(backoffInterval, 42);
    do_check_eq(Status.service, LOGIN_FAILED);
    do_check_eq(Status.login, SERVER_MAINTENANCE);
    do_check_false(errorHandler.didReportProlongedError);

    Svc.Obs.remove("weave:ui:login:error", onUIUpdate);
    clean();
    server.stop(deferred.resolve);
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  Service.sync();
  yield deferred.promise;
});

add_identity_test(this, function* test_meta_global_login_server_maintenance_error() {
  // Test meta/global server maintenance errors are not reported.
  let server = EHTestsCommon.sync_httpd_setup();
  yield EHTestsCommon.setUp(server);

  yield configureIdentity({username: "broken.meta"});
  Service.serverURL = server.baseURI + "/maintenance/";
  Service.clusterURL = server.baseURI + "/maintenance/";

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  function onUIUpdate() {
    do_throw("Shouldn't get here!");
  }
  Svc.Obs.add("weave:ui:login:error", onUIUpdate);

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  let deferred = Promise.defer();
  Svc.Obs.add("weave:ui:clear-error", function onLoginFinish() {
    Svc.Obs.remove("weave:ui:clear-error", onLoginFinish);

    do_check_true(Status.enforceBackoff);
    do_check_eq(backoffInterval, 42);
    do_check_eq(Status.service, LOGIN_FAILED);
    do_check_eq(Status.login, SERVER_MAINTENANCE);
    do_check_false(errorHandler.didReportProlongedError);

    Svc.Obs.remove("weave:ui:login:error", onUIUpdate);
    clean();
    server.stop(deferred.resolve);
  });

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  Service.sync();
  yield deferred.promise;
});
