/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");

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
    _("The 'nextSync' attribute stores a millisecond timestamp to the nearest second.");
    do_check_eq(SyncScheduler.nextSync, 0);
    SyncScheduler.nextSync = TIMESTAMP1;
    do_check_eq(SyncScheduler.nextSync, Math.floor(TIMESTAMP1/1000)*1000);

    _("'syncInterval' has a non-zero default value.");
    do_check_eq(Svc.Prefs.get('syncInterval'), undefined);
    do_check_true(SyncScheduler.syncInterval > 0);

    _("'syncInterval' corresponds to a preference setting.");
    SyncScheduler.syncInterval = INTERVAL;
    do_check_eq(SyncScheduler.syncInterval, INTERVAL);
    do_check_eq(Svc.Prefs.get('syncInterval'), INTERVAL);

    _("'syncInterval' ignored preference setting after partial sync..");
    Status.partial = true;
    do_check_eq(SyncScheduler.syncInterval, PARTIAL_DATA_SYNC);

    _("'syncThreshold' corresponds to preference, has non-zero default.");
    do_check_eq(Svc.Prefs.get('syncThreshold'), undefined);
    do_check_true(SyncScheduler.syncThreshold > 0);
    SyncScheduler.syncThreshold = THRESHOLD;
    do_check_eq(SyncScheduler.syncThreshold, THRESHOLD);
    do_check_eq(Svc.Prefs.get('syncThreshold'), THRESHOLD);

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

/**
 * Note: Sync interval tests to be added here in next patch.
 */
add_test(function test_updateClientMode() {
  _("Test updateClientMode switches sync threshold based on # of clients appropriately");
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);

  // Trigger a change in interval & threshold by adding a client.
  Clients._store._remoteClients.foo = "bar";
  SyncScheduler.updateClientMode();

  do_check_eq(SyncScheduler.syncThreshold, MULTI_DEVICE_THRESHOLD);

  // Resets the number of clients to 0. 
  Clients._store.wipe();
  SyncScheduler.updateClientMode();

  // Goes back to single user if # clients is 1.
  do_check_eq(Service.numClients, 1);
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  run_next_test();
});

add_test(function test_login_rejected_masterpassword_interval() {
  _("Test LOGIN_FAILED_LOGIN_REJECTED status results in reschedule at MASTER_PASSWORD interval");
  let loginFailed = false;
  Svc.Obs.add("weave:service:login:error", function() {
    loginFailed = true;
  });

  Service.username = "A";
  Service.password = "B";
  Service.passphrase = "C";

  let skipRetry = false;
  SyncScheduler.scheduleAtInterval = function (interval) {
    skipRetry = true;
    do_check_eq(interval, MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
  };

  // Will not sync due to not logged in.
  Service.sync();

  do_check_true(loginFailed);
  do_check_eq(Status.login, LOGIN_FAILED_LOGIN_REJECTED);
  do_check_true(skipRetry);
  run_next_test();
});

add_test(function test_calculateBackoff() {
  do_check_eq(Status.backoffInterval, 0);

  try {
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
  } finally {
    Status.backoffInterval = 0;
  }
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
  SyncScheduler._syncTimer.clear();


  _("Test setting sync interval when nextSync != 0");
  // Schedule next sync for some time in the future
  expectedInterval = 5000;
  initial_nextSync = SyncScheduler.nextSync = Date.now() + expectedInterval;
  SyncScheduler.scheduleNextSync();

  //Test nextSync value was changed.
  do_check_neq(SyncScheduler.nextSync, initial_nextSync);
  syncInterval = SyncScheduler.nextSync - Date.now();
  _("Sync Interval: " + syncInterval);

  // syncInterval is smaller than expectedInterval
  // since some time has passed.
  do_check_true(syncInterval <= expectedInterval);
  SyncScheduler._syncTimer.clear();

  run_next_test();
});

add_test(function test_handleSyncError() {
  let scheduleNextSyncCalled = 0;
  let scheduleAtIntervalCalled = 0;

  SyncScheduler.scheduleNextSync = function () {
    scheduleNextSyncCalled++;
  }
  
  SyncScheduler.scheduleAtInterval = function () {
    scheduleAtIntervalCalled++;
  }

  // Ensure expected initial environment.
  do_check_eq(SyncScheduler._syncErrors, 0);
  do_check_false(Status.enforceBackoff);

  // Call handleSyncError several times & ensure it
  // functions correctly.
  SyncScheduler.handleSyncError();
  do_check_eq(scheduleNextSyncCalled, 1);
  do_check_eq(scheduleAtIntervalCalled, 0);
  do_check_eq(SyncScheduler._syncErrors, 1);
  do_check_false(Status.enforceBackoff);

  SyncScheduler.handleSyncError();
  do_check_eq(scheduleNextSyncCalled, 2);
  do_check_eq(scheduleAtIntervalCalled, 0);
  do_check_eq(SyncScheduler._syncErrors, 2);
  do_check_false(Status.enforceBackoff);

  SyncScheduler.handleSyncError();
  do_check_eq(scheduleNextSyncCalled, 2);
  do_check_eq(scheduleAtIntervalCalled, 1);
  do_check_eq(SyncScheduler._syncErrors, 3);
  do_check_true(Status.enforceBackoff);
 
  // Status.enforceBackoff is false but there are still errors/
  Status.resetBackoff();
  do_check_false(Status.enforceBackoff);
  do_check_eq(SyncScheduler._syncErrors, 3);
  
  SyncScheduler.handleSyncError();
  do_check_eq(scheduleNextSyncCalled, 2);
  do_check_eq(scheduleAtIntervalCalled, 2);
  do_check_eq(SyncScheduler._syncErrors, 4);
  do_check_true(Status.enforceBackoff);

  run_next_test();
});
