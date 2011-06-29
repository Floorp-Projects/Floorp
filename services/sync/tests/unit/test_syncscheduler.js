/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/status.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");

function sync_httpd_setup() {
  let global = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {clients: {version: Clients.version,
                        syncID: Clients.syncID}}
  });
  let clientsColl = new ServerCollection({}, true);

  // Tracking info/collections.
  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;

  return httpd_setup({
    "/1.1/johndoe/storage/meta/global": upd("meta", global.handler()),
    "/1.1/johndoe/info/collections": collectionsHelper.handler,
    "/1.1/johndoe/storage/crypto/keys":
      upd("crypto", (new ServerWBO("keys")).handler()),
    "/1.1/johndoe/storage/clients": upd("clients", clientsColl.handler())
  });
}

function setUp() {
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "abcdeabcdeabcdeabcdeabcdea";
  Service.clusterURL = "http://localhost:8080/";

  generateNewKeys();
  let serverKeys = CollectionKeys.asWBO("crypto", "keys");
  serverKeys.encrypt(Service.syncKeyBundle);
  return serverKeys.upload(Service.cryptoKeysURL);
}

function run_test() {
  initTestLogging("Trace");

  Log4Moz.repository.getLogger("Sync.Service").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.SyncScheduler").level = Log4Moz.Level.Trace;

  run_next_test();
}

add_test(function test_prefAttributes() {
  _("Test various attributes corresponding to preferences.");

  const INTERVAL = 42 * 60 * 1000;   // 42 minutes
  const THRESHOLD = 3142;
  const SCORE = 2718;
  const TIMESTAMP1 = 1275493471649;

  try {
    _("'globalScore' corresponds to preference, defaults to zero.");
    do_check_eq(Svc.Prefs.get('globalScore'), undefined);
    do_check_eq(SyncScheduler.globalScore, 0);
    SyncScheduler.globalScore = SCORE;
    do_check_eq(SyncScheduler.globalScore, SCORE);
    do_check_eq(Svc.Prefs.get('globalScore'), SCORE);
  } finally {
    Svc.Prefs.resetBranch("");
    run_next_test();
  }
});

add_test(function test_updateClientMode() {
  _("Test updateClientMode adjusts scheduling attributes based on # of clients appropriately");
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  // Trigger a change in interval & threshold by adding a client.
  Clients._store.create({id: "foo", cleartext: "bar"});
  SyncScheduler.updateClientMode();

  do_check_eq(SyncScheduler.syncThreshold, MULTI_DEVICE_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_ACTIVE_SYNC);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  // Resets the number of clients to 0. 
  Clients.resetClient();
  SyncScheduler.updateClientMode();

  // Goes back to single user if # clients is 1.
  do_check_eq(SyncScheduler.numClients, 1);
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  Svc.Prefs.resetBranch("");
  SyncScheduler.setDefaults();
  Clients.resetClient();
  run_next_test();
});

add_test(function test_masterpassword_locked_retry_interval() {
  _("Test Status.login = MASTER_PASSWORD_LOCKED results in reschedule at MASTER_PASSWORD interval");
  let loginFailed = false;
  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError); 
    loginFailed = true;
  });

  let rescheduleInterval = false;
  SyncScheduler._scheduleAtInterval = SyncScheduler.scheduleAtInterval;
  SyncScheduler.scheduleAtInterval = function (interval) {
    rescheduleInterval = true;
    do_check_eq(interval, MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
  };

  Service._verifyLogin = Service.verifyLogin;
  Service.verifyLogin = function () {
    Status.login = MASTER_PASSWORD_LOCKED;
    return false;
  };

  let server = sync_httpd_setup();
  setUp();

  Service.sync();

  do_check_true(loginFailed);
  do_check_eq(Status.login, MASTER_PASSWORD_LOCKED);
  do_check_true(rescheduleInterval);

  Service.verifyLogin = Service._verifyLogin;
  SyncScheduler.scheduleAtInterval = SyncScheduler._scheduleAtInterval;

  Svc.Prefs.resetBranch("");
  SyncScheduler.setDefaults();
  Clients.resetClient();
  server.stop(run_next_test);
});

add_test(function test_calculateBackoff() {
  do_check_eq(Status.backoffInterval, 0);

  // Test no interval larger than the maximum backoff is used if
  // Status.backoffInterval is smaller.
  Status.backoffInterval = 5;
  let backoffInterval = Utils.calculateBackoff(50, MAXIMUM_BACKOFF_INTERVAL);

  do_check_eq(backoffInterval, MAXIMUM_BACKOFF_INTERVAL);

  // Test Status.backoffInterval is used if it is 
  // larger than MAXIMUM_BACKOFF_INTERVAL.
  Status.backoffInterval = MAXIMUM_BACKOFF_INTERVAL + 10;
  backoffInterval = Utils.calculateBackoff(50, MAXIMUM_BACKOFF_INTERVAL);
  
  do_check_eq(backoffInterval, MAXIMUM_BACKOFF_INTERVAL + 10);

  Status.backoffInterval = 0;
  Svc.Prefs.resetBranch("");
  SyncScheduler.setDefaults();
  Clients.resetClient();
  run_next_test();
});

add_test(function test_scheduleNextSync() {
  // The pre-set value for nextSync
  let initial_nextSync;

  // The estimated syncInterval value after calling scheduleNextSync.
  let syncInterval;

  // Test backoffInterval is 0 as expected.
  do_check_eq(Status.backoffInterval, 0);

  _("Test setting sync interval when nextSync == 0");
  initial_nextSync = SyncScheduler.nextSync = 0;
  let expectedInterval = SINGLE_USER_SYNC;
  SyncScheduler.scheduleNextSync();

  // Test nextSync value was changed.
  do_check_neq(SyncScheduler.nextSync, initial_nextSync);
  syncInterval = SyncScheduler.nextSync - Date.now();
  _("Sync Interval: " + syncInterval);

  // syncInterval is smaller than expectedInterval
  // since some time has passed.
  do_check_true(syncInterval <= expectedInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, expectedInterval);
  SyncScheduler.syncTimer.clear();


  _("Test setting sync interval when nextSync != 0");
  // Schedule next sync for some time in the future
  expectedInterval = 5000;
  initial_nextSync = SyncScheduler.nextSync = Date.now() + expectedInterval;
  SyncScheduler.scheduleNextSync();

  syncInterval = SyncScheduler.nextSync - Date.now();
  _("Sync Interval: " + syncInterval);

  // syncInterval is smaller than expectedInterval
  // since some time has passed.
  do_check_true(syncInterval <= expectedInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, expectedInterval);
  SyncScheduler.syncTimer.clear();

  SyncScheduler.setDefaults();
  run_next_test();
});

add_test(function test_handleSyncError() {
  let server = sync_httpd_setup();
  setUp();
 
  let origLockedSync = Service._lockedSync;
  Service._lockedSync = function () {
    // Force a sync fail.
    Service._loggedIn = false;
    origLockedSync.call(Service);
  };

  _("Ensure expected initial environment.");
  do_check_eq(SyncScheduler._syncErrors, 0);
  do_check_false(Status.enforceBackoff);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  do_check_eq(Status.backoffInterval, 0);

  // Trigger sync with an error several times & observe
  // functionality of handleSyncError()
  _("Test first error calls scheduleNextSync on default interval");
  Service.sync();
  do_check_true(SyncScheduler.nextSync <= Date.now() + SINGLE_USER_SYNC);
  do_check_eq(SyncScheduler.syncTimer.delay, SINGLE_USER_SYNC);
  do_check_eq(SyncScheduler._syncErrors, 1);
  do_check_false(Status.enforceBackoff);
  SyncScheduler.syncTimer.clear();

  _("Test second error still calls scheduleNextSync on default interval");
  Service.sync();
  do_check_true(SyncScheduler.nextSync <= Date.now() + SINGLE_USER_SYNC);
  do_check_eq(SyncScheduler.syncTimer.delay, SINGLE_USER_SYNC);
  do_check_eq(SyncScheduler._syncErrors, 2);
  do_check_false(Status.enforceBackoff);
  SyncScheduler.syncTimer.clear();

  _("Test third error sets Status.enforceBackoff and calls scheduleAtInterval");
  Service.sync();
  let maxInterval = SyncScheduler._syncErrors * (2 * MINIMUM_BACKOFF_INTERVAL);
  do_check_eq(Status.backoffInterval, 0);
  do_check_true(SyncScheduler.nextSync <= (Date.now() + maxInterval));
  do_check_true(SyncScheduler.syncTimer.delay <= maxInterval);
  do_check_eq(SyncScheduler._syncErrors, 3);
  do_check_true(Status.enforceBackoff);

  // Status.enforceBackoff is false but there are still errors.
  Status.resetBackoff();
  do_check_false(Status.enforceBackoff);
  do_check_eq(SyncScheduler._syncErrors, 3);
  SyncScheduler.syncTimer.clear();

  _("Test fourth error still calls scheduleAtInterval even if enforceBackoff was reset");
  Service.sync();
  maxInterval = SyncScheduler._syncErrors * (2 * MINIMUM_BACKOFF_INTERVAL);
  do_check_true(SyncScheduler.nextSync <= Date.now() + maxInterval);
  do_check_true(SyncScheduler.syncTimer.delay <= maxInterval);
  do_check_eq(SyncScheduler._syncErrors, 4);
  do_check_true(Status.enforceBackoff);
  SyncScheduler.syncTimer.clear();

  Service._lockedSync = origLockedSync;
  SyncScheduler.setDefaults();
  server.stop(run_next_test);
});

add_test(function test_client_sync_finish_updateClientMode() {
  let server = sync_httpd_setup();
  setUp();

  // Confirm defaults.
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  do_check_false(SyncScheduler.idle);

  // Trigger a change in interval & threshold by adding a client.
  Clients._store.create({id: "foo", cleartext: "bar"});
  do_check_false(SyncScheduler.numClients > 1);
  SyncScheduler.updateClientMode();
  Service.sync();

  do_check_eq(SyncScheduler.syncThreshold, MULTI_DEVICE_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_ACTIVE_SYNC);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  // Resets the number of clients to 0. 
  Clients.resetClient();
  Service.sync();

  // Goes back to single user if # clients is 1.
  do_check_eq(SyncScheduler.numClients, 1);
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  Svc.Prefs.resetBranch("");
  SyncScheduler.setDefaults();
  Clients.resetClient();
  server.stop(run_next_test);
});

add_test(function test_sync_at_startup() {
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);

    Svc.Prefs.resetBranch("");
    SyncScheduler.setDefaults();
    Clients.resetClient();

    server.stop(run_next_test);
  });

  let server = sync_httpd_setup();
  setUp();

  Service.delayedAutoConnect(0);
});

add_test(function test_idle_adjustSyncInterval() {
  // Confirm defaults.
  do_check_eq(SyncScheduler.idle, false);

  // Single device: nothing changes.
  SyncScheduler.observe(null, "idle", IDLE_TIME);
  do_check_eq(SyncScheduler.idle, true);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);

  // Multiple devices: switch to idle interval.
  SyncScheduler.idle = false;
  Clients._store.create({id: "foo", cleartext: "bar"});
  SyncScheduler.updateClientMode();
  SyncScheduler.observe(null, "idle", IDLE_TIME);
  do_check_eq(SyncScheduler.idle, true);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_IDLE_SYNC);

  SyncScheduler.setDefaults();
  run_next_test();
});
