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
Cu.import("resource://gre/modules/PromiseUtils.jsm");

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

  run_next_test();
}


function clean() {
  Service.startOver();
  Status.resetSync();
  Status.resetBackoff();
  errorHandler.didReportProlongedError = false;
}

add_identity_test(this, async function test_crypto_keys_login_server_maintenance_error() {
  Status.resetSync();
  // Test crypto/keys server maintenance errors are not reported.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.keys"}, server);

  // Force re-download of keys
  Service.collectionKeys.clear();

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

  let promiseObserved = promiseOneObserver("weave:ui:clear-error");

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  Service.sync();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  do_check_false(errorHandler.didReportProlongedError);

  Svc.Obs.remove("weave:ui:login:error", onUIUpdate);
  clean();
  await promiseStopServer(server);
});

add_task(async function test_sync_prolonged_server_maintenance_error() {
  // Test prolonged server maintenance errors are reported.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  const BACKOFF = 42;
  let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 503,
                      headers: {"retry-after": BACKOFF}};

  let promiseObserved = promiseOneObserver("weave:ui:sync:error");

  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  let ping = await sync_and_validate_telem(true);
  deepEqual(ping.status.sync, PROLONGED_SYNC_FAILURE);
  deepEqual(ping.engines.find(e => e.failureReason).failureReason,
            { name: "httperror", code: 503 });
  await promiseObserved;

  do_check_eq(Status.service, SYNC_FAILED);
  do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
  do_check_true(errorHandler.didReportProlongedError);

  await promiseStopServer(server);
  clean();
});

add_identity_test(this, async function test_info_collections_login_prolonged_server_maintenance_error() {
  // Test info/collections prolonged server maintenance errors are reported.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.info"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  Service.sync();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, SYNC_FAILED);
  do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
  do_check_true(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_meta_global_login_prolonged_server_maintenance_error() {
  // Test meta/global prolonged server maintenance errors are reported.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.meta"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  Service.sync();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, SYNC_FAILED);
  do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
  do_check_true(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_download_crypto_keys_login_prolonged_server_maintenance_error() {
  // Test crypto/keys prolonged server maintenance errors are reported.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.keys"}, server);
  // Force re-download of keys
  Service.collectionKeys.clear();

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  Service.sync();
  await promiseObserved;
  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, SYNC_FAILED);
  do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
  do_check_true(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_upload_crypto_keys_login_prolonged_server_maintenance_error() {
  // Test crypto/keys prolonged server maintenance errors are reported.
  let server = EHTestsCommon.sync_httpd_setup();

  // Start off with an empty account, do not upload a key.
  await configureIdentity({username: "broken.keys"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  Service.sync();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, SYNC_FAILED);
  do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
  do_check_true(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_wipeServer_login_prolonged_server_maintenance_error() {
  // Test that we report prolonged server maintenance errors that occur whilst
  // wiping the server.
  let server = EHTestsCommon.sync_httpd_setup();

  // Start off with an empty account, do not upload a key.
  await configureIdentity({username: "broken.wipe"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  Service.sync();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, SYNC_FAILED);
  do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
  do_check_true(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_wipeRemote_prolonged_server_maintenance_error() {
  // Test that we report prolonged server maintenance errors that occur whilst
  // wiping all remote devices.
  let server = EHTestsCommon.sync_httpd_setup();

  server.registerPathHandler("/1.1/broken.wipe/storage/catapult", EHTestsCommon.service_unavailable);
  await configureIdentity({username: "broken.wipe"}, server);
  EHTestsCommon.generateAndUploadKeys();

  let engine = engineManager.get("catapult");
  engine.exception = null;
  engine.enabled = true;

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:sync:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  Svc.Prefs.set("firstSync", "wipeRemote");
  setLastSync(PROLONGED_ERROR_DURATION);
  let ping = await sync_and_validate_telem(true);
  deepEqual(ping.failureReason, { name: "httperror", code: 503 });
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, SYNC_FAILED);
  do_check_eq(Status.sync, PROLONGED_SYNC_FAILURE);
  do_check_eq(Svc.Prefs.get("firstSync"), "wipeRemote");
  do_check_true(errorHandler.didReportProlongedError);
  await promiseStopServer(server);
  clean();
});

add_task(async function test_sync_syncAndReportErrors_server_maintenance_error() {
  // Test server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  const BACKOFF = 42;
  let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 503,
                      headers: {"retry-after": BACKOFF}};

  let promiseObserved = promiseOneObserver("weave:ui:sync:error");

  do_check_eq(Status.service, STATUS_OK);

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
  do_check_eq(Status.sync, SERVER_MAINTENANCE);
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_info_collections_login_syncAndReportErrors_server_maintenance_error() {
  // Test info/collections server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.info"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_meta_global_login_syncAndReportErrors_server_maintenance_error() {
  // Test meta/global server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.meta"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_download_crypto_keys_login_syncAndReportErrors_server_maintenance_error() {
  // Test crypto/keys server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.keys"}, server);
  // Force re-download of keys
  Service.collectionKeys.clear();

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_upload_crypto_keys_login_syncAndReportErrors_server_maintenance_error() {
  // Test crypto/keys server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();

  // Start off with an empty account, do not upload a key.
  await configureIdentity({username: "broken.keys"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_wipeServer_login_syncAndReportErrors_server_maintenance_error() {
  // Test crypto/keys server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();

  // Start off with an empty account, do not upload a key.
  await configureIdentity({username: "broken.wipe"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_wipeRemote_syncAndReportErrors_server_maintenance_error() {
  // Test that we report prolonged server maintenance errors that occur whilst
  // wiping all remote devices.
  let server = EHTestsCommon.sync_httpd_setup();

  await configureIdentity({username: "broken.wipe"}, server);
  EHTestsCommon.generateAndUploadKeys();

  let engine = engineManager.get("catapult");
  engine.exception = null;
  engine.enabled = true;

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:sync:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  Svc.Prefs.set("firstSync", "wipeRemote");
  setLastSync(NON_PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, SYNC_FAILED);
  do_check_eq(Status.sync, SERVER_MAINTENANCE);
  do_check_eq(Svc.Prefs.get("firstSync"), "wipeRemote");
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_task(async function test_sync_syncAndReportErrors_prolonged_server_maintenance_error() {
  // Test prolonged server maintenance errors are
  // reported when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  const BACKOFF = 42;
  let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 503,
                      headers: {"retry-after": BACKOFF}};

  let promiseObserved = promiseOneObserver("weave:ui:sync:error");

  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
  do_check_eq(Status.sync, SERVER_MAINTENANCE);
  // syncAndReportErrors means dontIgnoreErrors, which means
  // didReportProlongedError not touched.
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_info_collections_login_syncAndReportErrors_prolonged_server_maintenance_error() {
  // Test info/collections server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.info"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  // syncAndReportErrors means dontIgnoreErrors, which means
  // didReportProlongedError not touched.
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_meta_global_login_syncAndReportErrors_prolonged_server_maintenance_error() {
  // Test meta/global server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.meta"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  // syncAndReportErrors means dontIgnoreErrors, which means
  // didReportProlongedError not touched.
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_download_crypto_keys_login_syncAndReportErrors_prolonged_server_maintenance_error() {
  // Test crypto/keys server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();
  await EHTestsCommon.setUp(server);

  await configureIdentity({username: "broken.keys"}, server);
  // Force re-download of keys
  Service.collectionKeys.clear();

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  // syncAndReportErrors means dontIgnoreErrors, which means
  // didReportProlongedError not touched.
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_upload_crypto_keys_login_syncAndReportErrors_prolonged_server_maintenance_error() {
  // Test crypto/keys server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();

  // Start off with an empty account, do not upload a key.
  await configureIdentity({username: "broken.keys"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  // syncAndReportErrors means dontIgnoreErrors, which means
  // didReportProlongedError not touched.
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_identity_test(this, async function test_wipeServer_login_syncAndReportErrors_prolonged_server_maintenance_error() {
  // Test crypto/keys server maintenance errors are reported
  // when calling syncAndReportErrors.
  let server = EHTestsCommon.sync_httpd_setup();

  // Start off with an empty account, do not upload a key.
  await configureIdentity({username: "broken.wipe"}, server);

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
    Svc.Obs.remove("weave:service:backoff:interval", observe);
    backoffInterval = subject;
  });

  let promiseObserved = promiseOneObserver("weave:ui:login:error");

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.service, STATUS_OK);

  setLastSync(PROLONGED_ERROR_DURATION);
  errorHandler.syncAndReportErrors();
  await promiseObserved;

  do_check_true(Status.enforceBackoff);
  do_check_eq(backoffInterval, 42);
  do_check_eq(Status.service, LOGIN_FAILED);
  do_check_eq(Status.login, SERVER_MAINTENANCE);
  // syncAndReportErrors means dontIgnoreErrors, which means
  // didReportProlongedError not touched.
  do_check_false(errorHandler.didReportProlongedError);

  clean();
  await promiseStopServer(server);
});

add_task(async function test_sync_engine_generic_fail() {
  let server = EHTestsCommon.sync_httpd_setup();

let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.sync = function sync() {
    Svc.Obs.notify("weave:engine:sync:error", ENGINE_UNKNOWN_FAIL, "catapult");
  };

  let log = Log.repository.getLogger("Sync.ErrorHandler");
  Svc.Prefs.set("log.appender.file.logOnError", true);

  do_check_eq(Status.engines["catapult"], undefined);

  let deferred = PromiseUtils.defer();
  // Don't wait for reset-file-log until the sync is underway.
  // This avoids us catching a delayed notification from an earlier test.
  Svc.Obs.add("weave:engine:sync:finish", function onEngineFinish() {
    Svc.Obs.remove("weave:engine:sync:finish", onEngineFinish);

    log.info("Adding reset-file-log observer.");
    Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
      Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);

      // Put these checks here, not after sync(), so that we aren't racing the
      // log handler... which resets everything just a few lines below!
      _("Status.engines: " + JSON.stringify(Status.engines));
      do_check_eq(Status.engines["catapult"], ENGINE_UNKNOWN_FAIL);
      do_check_eq(Status.service, SYNC_FAILED_PARTIAL);

      // Test Error log was written on SYNC_FAILED_PARTIAL.
      let entries = logsdir.directoryEntries;
      do_check_true(entries.hasMoreElements());
      let logfile = entries.getNext().QueryInterface(Ci.nsILocalFile);
      do_check_true(logfile.leafName.startsWith("error-sync-"), logfile.leafName);

      clean();

      let syncErrors = sumHistogram("WEAVE_ENGINE_SYNC_ERRORS", { key: "catapult" });
      do_check_true(syncErrors, 1);

      server.stop(() => {
        clean();
        deferred.resolve();
      });
    });
  });

  do_check_true(await EHTestsCommon.setUp(server));
  let ping = await sync_and_validate_telem(true);
  deepEqual(ping.status.service, SYNC_FAILED_PARTIAL);
  deepEqual(ping.engines.find(e => e.status).status, ENGINE_UNKNOWN_FAIL);

  await deferred.promise;
});

add_test(function test_logs_on_sync_error_despite_shouldReportError() {
  _("Ensure that an error is still logged when weave:service:sync:error " +
    "is notified, despite shouldReportError returning false.");

  let log = Log.repository.getLogger("Sync.ErrorHandler");
  Svc.Prefs.set("log.appender.file.logOnError", true);
  log.info("TESTING");

  // Ensure that we report no error.
  Status.login = MASTER_PASSWORD_LOCKED;
  do_check_false(errorHandler.shouldReportError());

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);

    // Test that error log was written.
    let entries = logsdir.directoryEntries;
    do_check_true(entries.hasMoreElements());
    let logfile = entries.getNext().QueryInterface(Ci.nsILocalFile);
    do_check_true(logfile.leafName.startsWith("error-sync-"), logfile.leafName);

    clean();
    run_next_test();
  });
  Svc.Obs.notify("weave:service:sync:error", {});
});

add_test(function test_logs_on_login_error_despite_shouldReportError() {
  _("Ensure that an error is still logged when weave:service:login:error " +
    "is notified, despite shouldReportError returning false.");

  let log = Log.repository.getLogger("Sync.ErrorHandler");
  Svc.Prefs.set("log.appender.file.logOnError", true);
  log.info("TESTING");

  // Ensure that we report no error.
  Status.login = MASTER_PASSWORD_LOCKED;
  do_check_false(errorHandler.shouldReportError());

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);

    // Test that error log was written.
    let entries = logsdir.directoryEntries;
    do_check_true(entries.hasMoreElements());
    let logfile = entries.getNext().QueryInterface(Ci.nsILocalFile);
    do_check_true(logfile.leafName.startsWith("error-sync-"), logfile.leafName);

    clean();
    run_next_test();
  });
  Svc.Obs.notify("weave:service:login:error", {});
});

// This test should be the last one since it monkeypatches the engine object
// and we should only have one engine object throughout the file (bug 629664).
add_task(async function test_engine_applyFailed() {
  let server = EHTestsCommon.sync_httpd_setup();

  let engine = engineManager.get("catapult");
  engine.enabled = true;
  delete engine.exception;
  engine.sync = function sync() {
    Svc.Obs.notify("weave:engine:sync:applied", {newFailed:1}, "catapult");
  };

  let log = Log.repository.getLogger("Sync.ErrorHandler");
  Svc.Prefs.set("log.appender.file.logOnError", true);

  let promiseObserved = promiseOneObserver("weave:service:reset-file-log");

  do_check_eq(Status.engines["catapult"], undefined);
  do_check_true(await EHTestsCommon.setUp(server));
  Service.sync();
  await promiseObserved;

  do_check_eq(Status.engines["catapult"], ENGINE_APPLY_FAIL);
  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);

  // Test Error log was written on SYNC_FAILED_PARTIAL.
  let entries = logsdir.directoryEntries;
  do_check_true(entries.hasMoreElements());
  let logfile = entries.getNext().QueryInterface(Ci.nsILocalFile);
  do_check_true(logfile.leafName.startsWith("error-sync-"), logfile.leafName);

  clean();
  await promiseStopServer(server);
});
