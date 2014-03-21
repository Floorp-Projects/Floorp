/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

Service.engineManager.clear();

function CatapultEngine() {
  SyncEngine.call(this, "Catapult", Service);
}
CatapultEngine.prototype = {
  __proto__: SyncEngine.prototype,
  exception: null, // tests fill this in
  _sync: function _sync() {
    throw this.exception;
  }
};

Service.engineManager.register(CatapultEngine);

let scheduler = new SyncScheduler(Service);
let clientsEngine = Service.clientsEngine;

function sync_httpd_setup() {
  let global = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {clients: {version: clientsEngine.version,
                        syncID: clientsEngine.syncID}}
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
    "/1.1/johndoe/storage/clients": upd("clients", clientsColl.handler()),
    "/user/1.0/johndoe/node/weave": httpd_handler(200, "OK", "null")
  });
}

function setUp(server) {
  let deferred = Promise.defer();
  configureIdentity({username: "johndoe"}).then(() => {
    Service.clusterURL = server.baseURI + "/";

    generateNewKeys(Service.collectionKeys);
    let serverKeys = Service.collectionKeys.asWBO("crypto", "keys");
    serverKeys.encrypt(Service.identity.syncKeyBundle);
    let result = serverKeys.upload(Service.resource(Service.cryptoKeysURL)).success;
    deferred.resolve(result);
  });
  return deferred.promise;
}

function cleanUpAndGo(server) {
  let deferred = Promise.defer();
  Utils.nextTick(function () {
    Service.startOver();
    if (server) {
      server.stop(deferred.resolve);
    } else {
      deferred.resolve();
    }
  });
  return deferred.promise;
}

function run_test() {
  initTestLogging("Trace");

  Log.repository.getLogger("Sync.Service").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.scheduler").level = Log.Level.Trace;

  // The scheduler checks Weave.fxaEnabled to determine whether to use
  // FxA defaults or legacy defaults.  As .fxaEnabled checks the username, we
  // set a username here then reset the default to ensure they are used.
  ensureLegacyIdentityManager();
  setBasicCredentials("johndoe");
  scheduler.setDefaults();

  run_next_test();
}

add_test(function test_prefAttributes() {
  _("Test various attributes corresponding to preferences.");

  const INTERVAL = 42 * 60 * 1000;   // 42 minutes
  const THRESHOLD = 3142;
  const SCORE = 2718;
  const TIMESTAMP1 = 1275493471649;

  _("The 'nextSync' attribute stores a millisecond timestamp rounded down to the nearest second.");
  do_check_eq(scheduler.nextSync, 0);
  scheduler.nextSync = TIMESTAMP1;
  do_check_eq(scheduler.nextSync, Math.floor(TIMESTAMP1 / 1000) * 1000);

  _("'syncInterval' defaults to singleDeviceInterval.");
  do_check_eq(Svc.Prefs.get('syncInterval'), undefined);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  _("'syncInterval' corresponds to a preference setting.");
  scheduler.syncInterval = INTERVAL;
  do_check_eq(scheduler.syncInterval, INTERVAL);
  do_check_eq(Svc.Prefs.get('syncInterval'), INTERVAL);

  _("'syncThreshold' corresponds to preference, defaults to SINGLE_USER_THRESHOLD");
  do_check_eq(Svc.Prefs.get('syncThreshold'), undefined);
  do_check_eq(scheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  scheduler.syncThreshold = THRESHOLD;
  do_check_eq(scheduler.syncThreshold, THRESHOLD);

  _("'globalScore' corresponds to preference, defaults to zero.");
  do_check_eq(Svc.Prefs.get('globalScore'), 0);
  do_check_eq(scheduler.globalScore, 0);
  scheduler.globalScore = SCORE;
  do_check_eq(scheduler.globalScore, SCORE);
  do_check_eq(Svc.Prefs.get('globalScore'), SCORE);

  _("Intervals correspond to default preferences.");
  do_check_eq(scheduler.singleDeviceInterval,
              Svc.Prefs.get("scheduler.sync11.singleDeviceInterval") * 1000);
  do_check_eq(scheduler.idleInterval,
              Svc.Prefs.get("scheduler.idleInterval") * 1000);
  do_check_eq(scheduler.activeInterval,
              Svc.Prefs.get("scheduler.activeInterval") * 1000);
  do_check_eq(scheduler.immediateInterval,
              Svc.Prefs.get("scheduler.immediateInterval") * 1000);

  _("Custom values for prefs will take effect after a restart.");
  Svc.Prefs.set("scheduler.sync11.singleDeviceInterval", 42);
  Svc.Prefs.set("scheduler.idleInterval", 23);
  Svc.Prefs.set("scheduler.activeInterval", 18);
  Svc.Prefs.set("scheduler.immediateInterval", 31415);
  scheduler.setDefaults();
  do_check_eq(scheduler.idleInterval, 23000);
  do_check_eq(scheduler.singleDeviceInterval, 42000);
  do_check_eq(scheduler.activeInterval, 18000);
  do_check_eq(scheduler.immediateInterval, 31415000);

  Svc.Prefs.resetBranch("");
  scheduler.setDefaults();
  run_next_test();
});

add_identity_test(this, function test_updateClientMode() {
  _("Test updateClientMode adjusts scheduling attributes based on # of clients appropriately");
  do_check_eq(scheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
  do_check_false(scheduler.numClients > 1);
  do_check_false(scheduler.idle);

  // Trigger a change in interval & threshold by adding a client.
  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  scheduler.updateClientMode();

  do_check_eq(scheduler.syncThreshold, MULTI_DEVICE_THRESHOLD);
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);
  do_check_true(scheduler.numClients > 1);
  do_check_false(scheduler.idle);

  // Resets the number of clients to 0.
  clientsEngine.resetClient();
  scheduler.updateClientMode();

  // Goes back to single user if # clients is 1.
  do_check_eq(scheduler.numClients, 1);
  do_check_eq(scheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
  do_check_false(scheduler.numClients > 1);
  do_check_false(scheduler.idle);

  yield cleanUpAndGo();
});

add_identity_test(this, function test_masterpassword_locked_retry_interval() {
  _("Test Status.login = MASTER_PASSWORD_LOCKED results in reschedule at MASTER_PASSWORD interval");
  let loginFailed = false;
  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    loginFailed = true;
  });

  let rescheduleInterval = false;

  let oldScheduleAtInterval = SyncScheduler.prototype.scheduleAtInterval;
  SyncScheduler.prototype.scheduleAtInterval = function (interval) {
    rescheduleInterval = true;
    do_check_eq(interval, MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
  };

  let oldVerifyLogin = Service.verifyLogin;
  Service.verifyLogin = function () {
    Status.login = MASTER_PASSWORD_LOCKED;
    return false;
  };

  let server = sync_httpd_setup();
  yield setUp(server);

  Service.sync();

  do_check_true(loginFailed);
  do_check_eq(Status.login, MASTER_PASSWORD_LOCKED);
  do_check_true(rescheduleInterval);

  Service.verifyLogin = oldVerifyLogin;
  SyncScheduler.prototype.scheduleAtInterval = oldScheduleAtInterval;

  yield cleanUpAndGo(server);
});

add_identity_test(this, function test_calculateBackoff() {
  do_check_eq(Status.backoffInterval, 0);

  // Test no interval larger than the maximum backoff is used if
  // Status.backoffInterval is smaller.
  Status.backoffInterval = 5;
  let backoffInterval = Utils.calculateBackoff(50, MAXIMUM_BACKOFF_INTERVAL,
                                               Status.backoffInterval);

  do_check_eq(backoffInterval, MAXIMUM_BACKOFF_INTERVAL);

  // Test Status.backoffInterval is used if it is
  // larger than MAXIMUM_BACKOFF_INTERVAL.
  Status.backoffInterval = MAXIMUM_BACKOFF_INTERVAL + 10;
  backoffInterval = Utils.calculateBackoff(50, MAXIMUM_BACKOFF_INTERVAL,
                                           Status.backoffInterval);

  do_check_eq(backoffInterval, MAXIMUM_BACKOFF_INTERVAL + 10);

  yield cleanUpAndGo();
});

add_identity_test(this, function test_scheduleNextSync_nowOrPast() {
  let deferred = Promise.defer();
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
    cleanUpAndGo(server).then(deferred.resolve);
  });

  let server = sync_httpd_setup();
  yield setUp(server);

  // We're late for a sync...
  scheduler.scheduleNextSync(-1);
  yield deferred.promise;
});

add_identity_test(this, function test_scheduleNextSync_future_noBackoff() {
  _("scheduleNextSync() uses the current syncInterval if no interval is provided.");
  // Test backoffInterval is 0 as expected.
  do_check_eq(Status.backoffInterval, 0);

  _("Test setting sync interval when nextSync == 0");
  scheduler.nextSync = 0;
  scheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(scheduler.nextSync - Date.now()
                <= scheduler.syncInterval);
  do_check_eq(scheduler.syncTimer.delay, scheduler.syncInterval);

  _("Test setting sync interval when nextSync != 0");
  scheduler.nextSync = Date.now() + scheduler.singleDeviceInterval;
  scheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(scheduler.nextSync - Date.now()
                <= scheduler.syncInterval);
  do_check_true(scheduler.syncTimer.delay <= scheduler.syncInterval);

  _("Scheduling requests for intervals larger than the current one will be ignored.");
  // Request a sync at a longer interval. The sync that's already scheduled
  // for sooner takes precedence.
  let nextSync = scheduler.nextSync;
  let timerDelay = scheduler.syncTimer.delay;
  let requestedInterval = scheduler.syncInterval * 10;
  scheduler.scheduleNextSync(requestedInterval);
  do_check_eq(scheduler.nextSync, nextSync);
  do_check_eq(scheduler.syncTimer.delay, timerDelay);

  // We can schedule anything we want if there isn't a sync scheduled.
  scheduler.nextSync = 0;
  scheduler.scheduleNextSync(requestedInterval);
  do_check_true(scheduler.nextSync <= Date.now() + requestedInterval);
  do_check_eq(scheduler.syncTimer.delay, requestedInterval);

  // Request a sync at the smallest possible interval (0 triggers now).
  scheduler.scheduleNextSync(1);
  do_check_true(scheduler.nextSync <= Date.now() + 1);
  do_check_eq(scheduler.syncTimer.delay, 1);

  yield cleanUpAndGo();
});

add_identity_test(this, function test_scheduleNextSync_future_backoff() {
 _("scheduleNextSync() will honour backoff in all scheduling requests.");
  // Let's take a backoff interval that's bigger than the default sync interval.
  const BACKOFF = 7337;
  Status.backoffInterval = scheduler.syncInterval + BACKOFF;

  _("Test setting sync interval when nextSync == 0");
  scheduler.nextSync = 0;
  scheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(scheduler.nextSync - Date.now()
                <= Status.backoffInterval);
  do_check_eq(scheduler.syncTimer.delay, Status.backoffInterval);

  _("Test setting sync interval when nextSync != 0");
  scheduler.nextSync = Date.now() + scheduler.singleDeviceInterval;
  scheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(scheduler.nextSync - Date.now()
                <= Status.backoffInterval);
  do_check_true(scheduler.syncTimer.delay <= Status.backoffInterval);

  // Request a sync at a longer interval. The sync that's already scheduled
  // for sooner takes precedence.
  let nextSync = scheduler.nextSync;
  let timerDelay = scheduler.syncTimer.delay;
  let requestedInterval = scheduler.syncInterval * 10;
  do_check_true(requestedInterval > Status.backoffInterval);
  scheduler.scheduleNextSync(requestedInterval);
  do_check_eq(scheduler.nextSync, nextSync);
  do_check_eq(scheduler.syncTimer.delay, timerDelay);

  // We can schedule anything we want if there isn't a sync scheduled.
  scheduler.nextSync = 0;
  scheduler.scheduleNextSync(requestedInterval);
  do_check_true(scheduler.nextSync <= Date.now() + requestedInterval);
  do_check_eq(scheduler.syncTimer.delay, requestedInterval);

  // Request a sync at the smallest possible interval (0 triggers now).
  scheduler.scheduleNextSync(1);
  do_check_true(scheduler.nextSync <= Date.now() + Status.backoffInterval);
  do_check_eq(scheduler.syncTimer.delay, Status.backoffInterval);

  yield cleanUpAndGo();
});

add_identity_test(this, function test_handleSyncError() {
  let server = sync_httpd_setup();
  yield setUp(server);

  // Force sync to fail.
  Svc.Prefs.set("firstSync", "notReady");

  _("Ensure expected initial environment.");
  do_check_eq(scheduler._syncErrors, 0);
  do_check_false(Status.enforceBackoff);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
  do_check_eq(Status.backoffInterval, 0);

  // Trigger sync with an error several times & observe
  // functionality of handleSyncError()
  _("Test first error calls scheduleNextSync on default interval");
  Service.sync();
  do_check_true(scheduler.nextSync <= Date.now() + scheduler.singleDeviceInterval);
  do_check_eq(scheduler.syncTimer.delay, scheduler.singleDeviceInterval);
  do_check_eq(scheduler._syncErrors, 1);
  do_check_false(Status.enforceBackoff);
  scheduler.syncTimer.clear();

  _("Test second error still calls scheduleNextSync on default interval");
  Service.sync();
  do_check_true(scheduler.nextSync <= Date.now() + scheduler.singleDeviceInterval);
  do_check_eq(scheduler.syncTimer.delay, scheduler.singleDeviceInterval);
  do_check_eq(scheduler._syncErrors, 2);
  do_check_false(Status.enforceBackoff);
  scheduler.syncTimer.clear();

  _("Test third error sets Status.enforceBackoff and calls scheduleAtInterval");
  Service.sync();
  let maxInterval = scheduler._syncErrors * (2 * MINIMUM_BACKOFF_INTERVAL);
  do_check_eq(Status.backoffInterval, 0);
  do_check_true(scheduler.nextSync <= (Date.now() + maxInterval));
  do_check_true(scheduler.syncTimer.delay <= maxInterval);
  do_check_eq(scheduler._syncErrors, 3);
  do_check_true(Status.enforceBackoff);

  // Status.enforceBackoff is false but there are still errors.
  Status.resetBackoff();
  do_check_false(Status.enforceBackoff);
  do_check_eq(scheduler._syncErrors, 3);
  scheduler.syncTimer.clear();

  _("Test fourth error still calls scheduleAtInterval even if enforceBackoff was reset");
  Service.sync();
  maxInterval = scheduler._syncErrors * (2 * MINIMUM_BACKOFF_INTERVAL);
  do_check_true(scheduler.nextSync <= Date.now() + maxInterval);
  do_check_true(scheduler.syncTimer.delay <= maxInterval);
  do_check_eq(scheduler._syncErrors, 4);
  do_check_true(Status.enforceBackoff);
  scheduler.syncTimer.clear();

  _("Arrange for a successful sync to reset the scheduler error count");
  let deferred = Promise.defer();
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
    cleanUpAndGo(server).then(deferred.resolve);
  });
  Svc.Prefs.set("firstSync", "wipeRemote");
  scheduler.scheduleNextSync(-1);
  yield deferred.promise;
});

add_identity_test(this, function test_client_sync_finish_updateClientMode() {
  let server = sync_httpd_setup();
  yield setUp(server);

  // Confirm defaults.
  do_check_eq(scheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
  do_check_false(scheduler.idle);

  // Trigger a change in interval & threshold by adding a client.
  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  do_check_false(scheduler.numClients > 1);
  scheduler.updateClientMode();
  Service.sync();

  do_check_eq(scheduler.syncThreshold, MULTI_DEVICE_THRESHOLD);
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);
  do_check_true(scheduler.numClients > 1);
  do_check_false(scheduler.idle);

  // Resets the number of clients to 0.
  clientsEngine.resetClient();
  Service.sync();

  // Goes back to single user if # clients is 1.
  do_check_eq(scheduler.numClients, 1);
  do_check_eq(scheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
  do_check_false(scheduler.numClients > 1);
  do_check_false(scheduler.idle);

  yield cleanUpAndGo(server);
});

add_identity_test(this, function test_autoconnect_nextSync_past() {
  let deferred = Promise.defer();
  // nextSync will be 0 by default, so it's way in the past.

  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
    cleanUpAndGo(server).then(deferred.resolve);
  });

  let server = sync_httpd_setup();
  yield setUp(server);

  scheduler.delayedAutoConnect(0);
  yield deferred.promise;
});

add_identity_test(this, function test_autoconnect_nextSync_future() {
  let deferred = Promise.defer();
  let previousSync = Date.now() + scheduler.syncInterval / 2;
  scheduler.nextSync = previousSync;
  // nextSync rounds to the nearest second.
  let expectedSync = scheduler.nextSync;
  let expectedInterval = expectedSync - Date.now() - 1000;

  // Ensure we don't actually try to sync (or log in for that matter).
  function onLoginStart() {
    do_throw("Should not get here!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  waitForZeroTimer(function () {
    do_check_eq(scheduler.nextSync, expectedSync);
    do_check_true(scheduler.syncTimer.delay >= expectedInterval);

    Svc.Obs.remove("weave:service:login:start", onLoginStart);
    cleanUpAndGo().then(deferred.resolve);
  });

  yield configureIdentity({username: "johndoe"});
  scheduler.delayedAutoConnect(0);
  yield deferred.promise;
});

// XXX - this test can't be run with the browserid identity as it relies
// on the syncKey getter behaving in a certain way...
add_task(function test_autoconnect_mp_locked() {
  let server = sync_httpd_setup();
  yield setUp(server);

  // Pretend user did not unlock master password.
  let origLocked = Utils.mpLocked;
  Utils.mpLocked = function() true;

  let origGetter = Service.identity.__lookupGetter__("syncKey");
  let origSetter = Service.identity.__lookupSetter__("syncKey");
  delete Service.identity.syncKey;
  Service.identity.__defineGetter__("syncKey", function() {
    _("Faking Master Password entry cancelation.");
    throw "User canceled Master Password entry";
  });

  let deferred = Promise.defer();
  // A locked master password will still trigger a sync, but then we'll hit
  // MASTER_PASSWORD_LOCKED and hence MASTER_PASSWORD_LOCKED_RETRY_INTERVAL.
  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    Utils.nextTick(function aLittleBitAfterLoginError() {
      do_check_eq(Status.login, MASTER_PASSWORD_LOCKED);

      Utils.mpLocked = origLocked;
      delete Service.identity.syncKey;
      Service.identity.__defineGetter__("syncKey", origGetter);
      Service.identity.__defineSetter__("syncKey", origSetter);

      cleanUpAndGo(server).then(deferred.resolve);
    });
  });

  scheduler.delayedAutoConnect(0);
  yield deferred.promise;
});

add_identity_test(this, function test_no_autoconnect_during_wizard() {
  let server = sync_httpd_setup();
  yield setUp(server);

  // Simulate the Sync setup wizard.
  Svc.Prefs.set("firstSync", "notReady");

  // Ensure we don't actually try to sync (or log in for that matter).
  function onLoginStart() {
    do_throw("Should not get here!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  let deferred = Promise.defer();
  waitForZeroTimer(function () {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);
    cleanUpAndGo(server).then(deferred.resolve);
  });

  scheduler.delayedAutoConnect(0);
  yield deferred.promise;
});

add_identity_test(this, function test_no_autoconnect_status_not_ok() {
  let server = sync_httpd_setup();

  // Ensure we don't actually try to sync (or log in for that matter).
  function onLoginStart() {
    do_throw("Should not get here!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  let deferred = Promise.defer();
  waitForZeroTimer(function () {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);

    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);

    cleanUpAndGo(server).then(deferred.resolve);
  });

  scheduler.delayedAutoConnect(0);
  yield deferred.promise;
});

add_identity_test(this, function test_autoconnectDelay_pref() {
  let deferred = Promise.defer();
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
    cleanUpAndGo(server).then(deferred.resolve);
  });

  Svc.Prefs.set("autoconnectDelay", 1);

  let server = sync_httpd_setup();
  yield setUp(server);

  Svc.Obs.notify("weave:service:ready");

  // autoconnectDelay pref is multiplied by 1000.
  do_check_eq(scheduler._autoTimer.delay, 1000);
  do_check_eq(Status.service, STATUS_OK);
  yield deferred.promise;
});

add_identity_test(this, function test_idle_adjustSyncInterval() {
  // Confirm defaults.
  do_check_eq(scheduler.idle, false);

  // Single device: nothing changes.
  scheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_eq(scheduler.idle, true);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  // Multiple devices: switch to idle interval.
  scheduler.idle = false;
  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  scheduler.updateClientMode();
  scheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_eq(scheduler.idle, true);
  do_check_eq(scheduler.syncInterval, scheduler.idleInterval);

  yield cleanUpAndGo();
});

add_identity_test(this, function test_back_triggersSync() {
  // Confirm defaults.
  do_check_false(scheduler.idle);
  do_check_eq(Status.backoffInterval, 0);

  // Set up: Define 2 clients and put the system in idle.
  scheduler.numClients = 2;
  scheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_true(scheduler.idle);

  let deferred = Promise.defer();
  // We don't actually expect the sync (or the login, for that matter) to
  // succeed. We just want to ensure that it was attempted.
  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    cleanUpAndGo().then(deferred.resolve);
  });

  // Send an 'active' event to trigger sync soonish.
  scheduler.observe(null, "active", Svc.Prefs.get("scheduler.idleTime"));
  yield deferred.promise;
});

add_identity_test(this, function test_active_triggersSync_observesBackoff() {
  // Confirm defaults.
  do_check_false(scheduler.idle);

  // Set up: Set backoff, define 2 clients and put the system in idle.
  const BACKOFF = 7337;
  Status.backoffInterval = scheduler.idleInterval + BACKOFF;
  scheduler.numClients = 2;
  scheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_eq(scheduler.idle, true);

  function onLoginStart() {
    do_throw("Shouldn't have kicked off a sync!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  let deferred = Promise.defer();
  timer = Utils.namedTimer(function () {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);

    do_check_true(scheduler.nextSync <= Date.now() + Status.backoffInterval);
    do_check_eq(scheduler.syncTimer.delay, Status.backoffInterval);

    cleanUpAndGo().then(deferred.resolve);
  }, IDLE_OBSERVER_BACK_DELAY * 1.5, {}, "timer");

  // Send an 'active' event to try to trigger sync soonish.
  scheduler.observe(null, "active", Svc.Prefs.get("scheduler.idleTime"));
  yield deferred.promise;
});

add_identity_test(this, function test_back_debouncing() {
  _("Ensure spurious back-then-idle events, as observed on OS X, don't trigger a sync.");

  // Confirm defaults.
  do_check_eq(scheduler.idle, false);

  // Set up: Define 2 clients and put the system in idle.
  scheduler.numClients = 2;
  scheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_eq(scheduler.idle, true);

  function onLoginStart() {
    do_throw("Shouldn't have kicked off a sync!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  // Create spurious back-then-idle events as observed on OS X:
  scheduler.observe(null, "active", Svc.Prefs.get("scheduler.idleTime"));
  scheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));

  let deferred = Promise.defer();
  timer = Utils.namedTimer(function () {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);
    cleanUpAndGo().then(deferred.resolve);
  }, IDLE_OBSERVER_BACK_DELAY * 1.5, {}, "timer");
  yield deferred.promise;
});

add_identity_test(this, function test_no_sync_node() {
  // Test when Status.sync == NO_SYNC_NODE_FOUND
  // it is not overwritten on sync:finish
  let server = sync_httpd_setup();
  yield setUp(server);

  Service.serverURL = server.baseURI + "/";

  Service.sync();
  do_check_eq(Status.sync, NO_SYNC_NODE_FOUND);
  do_check_eq(scheduler.syncTimer.delay, NO_SYNC_NODE_INTERVAL);

  yield cleanUpAndGo(server);
});

add_identity_test(this, function test_sync_failed_partial_500s() {
  _("Test a 5xx status calls handleSyncError.");
  scheduler._syncErrors = MAX_ERROR_COUNT_BEFORE_BACKOFF;
  let server = sync_httpd_setup();

  let engine = Service.engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 500};

  do_check_eq(Status.sync, SYNC_SUCCEEDED);

  do_check_true(yield setUp(server));

  Service.sync();

  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);

  let maxInterval = scheduler._syncErrors * (2 * MINIMUM_BACKOFF_INTERVAL);
  do_check_eq(Status.backoffInterval, 0);
  do_check_true(Status.enforceBackoff);
  do_check_eq(scheduler._syncErrors, 4);
  do_check_true(scheduler.nextSync <= (Date.now() + maxInterval));
  do_check_true(scheduler.syncTimer.delay <= maxInterval);

  yield cleanUpAndGo(server);
});

add_identity_test(this, function test_sync_failed_partial_400s() {
  _("Test a non-5xx status doesn't call handleSyncError.");
  scheduler._syncErrors = MAX_ERROR_COUNT_BEFORE_BACKOFF;
  let server = sync_httpd_setup();

  let engine = Service.engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 400};

  // Have multiple devices for an active interval.
  clientsEngine._store.create({id: "foo", cleartext: "bar"});

  do_check_eq(Status.sync, SYNC_SUCCEEDED);

  do_check_true(yield setUp(server));

  Service.sync();

  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);

  do_check_eq(Status.backoffInterval, 0);
  do_check_false(Status.enforceBackoff);
  do_check_eq(scheduler._syncErrors, 0);
  do_check_true(scheduler.nextSync <= (Date.now() + scheduler.activeInterval));
  do_check_true(scheduler.syncTimer.delay <= scheduler.activeInterval);

  yield cleanUpAndGo(server);
});

add_identity_test(this, function test_sync_X_Weave_Backoff() {
  let server = sync_httpd_setup();
  yield setUp(server);

  // Use an odd value on purpose so that it doesn't happen to coincide with one
  // of the sync intervals.
  const BACKOFF = 7337;

  // Extend info/collections so that we can put it into server maintenance mode.
  const INFO_COLLECTIONS = "/1.1/johndoe/info/collections";
  let infoColl = server._handler._overridePaths[INFO_COLLECTIONS];
  let serverBackoff = false;
  function infoCollWithBackoff(request, response) {
    if (serverBackoff) {
      response.setHeader("X-Weave-Backoff", "" + BACKOFF);
    }
    infoColl(request, response);
  }
  server.registerPathHandler(INFO_COLLECTIONS, infoCollWithBackoff);

  // Pretend we have two clients so that the regular sync interval is
  // sufficiently low.
  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  let rec = clientsEngine._store.createRecord("foo", "clients");
  rec.encrypt(Service.collectionKeys.keyForCollection("clients"));
  rec.upload(Service.resource(clientsEngine.engineURL + rec.id));

  // Sync once to log in and get everything set up. Let's verify our initial
  // values.
  Service.sync();
  do_check_eq(Status.backoffInterval, 0);
  do_check_eq(Status.minimumNextSync, 0);
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);
  do_check_true(scheduler.nextSync <=
                Date.now() + scheduler.syncInterval);
  // Sanity check that we picked the right value for BACKOFF:
  do_check_true(scheduler.syncInterval < BACKOFF * 1000);

  // Turn on server maintenance and sync again.
  serverBackoff = true;
  Service.sync();

  do_check_true(Status.backoffInterval >= BACKOFF * 1000);
  // Allowing 1 second worth of of leeway between when Status.minimumNextSync
  // was set and when this line gets executed.
  let minimumExpectedDelay = (BACKOFF - 1) * 1000;
  do_check_true(Status.minimumNextSync >= Date.now() + minimumExpectedDelay);

  // Verify that the next sync is actually going to wait that long.
  do_check_true(scheduler.nextSync >= Date.now() + minimumExpectedDelay);
  do_check_true(scheduler.syncTimer.delay >= minimumExpectedDelay);

  yield cleanUpAndGo(server);
});

add_identity_test(this, function test_sync_503_Retry_After() {
  let server = sync_httpd_setup();
  yield setUp(server);

  // Use an odd value on purpose so that it doesn't happen to coincide with one
  // of the sync intervals.
  const BACKOFF = 7337;

  // Extend info/collections so that we can put it into server maintenance mode.
  const INFO_COLLECTIONS = "/1.1/johndoe/info/collections";
  let infoColl = server._handler._overridePaths[INFO_COLLECTIONS];
  let serverMaintenance = false;
  function infoCollWithMaintenance(request, response) {
    if (!serverMaintenance) {
      infoColl(request, response);
      return;
    }
    response.setHeader("Retry-After", "" + BACKOFF);
    response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
  }
  server.registerPathHandler(INFO_COLLECTIONS, infoCollWithMaintenance);

  // Pretend we have two clients so that the regular sync interval is
  // sufficiently low.
  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  let rec = clientsEngine._store.createRecord("foo", "clients");
  rec.encrypt(Service.collectionKeys.keyForCollection("clients"));
  rec.upload(Service.resource(clientsEngine.engineURL + rec.id));

  // Sync once to log in and get everything set up. Let's verify our initial
  // values.
  Service.sync();
  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.backoffInterval, 0);
  do_check_eq(Status.minimumNextSync, 0);
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);
  do_check_true(scheduler.nextSync <=
                Date.now() + scheduler.syncInterval);
  // Sanity check that we picked the right value for BACKOFF:
  do_check_true(scheduler.syncInterval < BACKOFF * 1000);

  // Turn on server maintenance and sync again.
  serverMaintenance = true;
  Service.sync();

  do_check_true(Status.enforceBackoff);
  do_check_true(Status.backoffInterval >= BACKOFF * 1000);
  // Allowing 1 second worth of of leeway between when Status.minimumNextSync
  // was set and when this line gets executed.
  let minimumExpectedDelay = (BACKOFF - 1) * 1000;
  do_check_true(Status.minimumNextSync >= Date.now() + minimumExpectedDelay);

  // Verify that the next sync is actually going to wait that long.
  do_check_true(scheduler.nextSync >= Date.now() + minimumExpectedDelay);
  do_check_true(scheduler.syncTimer.delay >= minimumExpectedDelay);

  yield cleanUpAndGo(server);
});

add_identity_test(this, function test_loginError_recoverable_reschedules() {
  _("Verify that a recoverable login error schedules a new sync.");
  yield configureIdentity({username: "johndoe"});
  Service.serverURL = "http://localhost:1234/";
  Service.clusterURL = Service.serverURL;
  Service.persistLogin();
  Status.resetSync(); // reset Status.login

  let deferred = Promise.defer();
  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    Utils.nextTick(function aLittleBitAfterLoginError() {
      do_check_eq(Status.login, LOGIN_FAILED_NETWORK_ERROR);

      let expectedNextSync = Date.now() + scheduler.syncInterval;
      do_check_true(scheduler.nextSync > Date.now());
      do_check_true(scheduler.nextSync <= expectedNextSync);
      do_check_true(scheduler.syncTimer.delay > 0);
      do_check_true(scheduler.syncTimer.delay <= scheduler.syncInterval);

      Svc.Obs.remove("weave:service:sync:start", onSyncStart);
      cleanUpAndGo().then(deferred.resolve);
    });
  });

  // Let's set it up so that a sync is overdue, both in terms of previously
  // scheduled syncs and the global score. We still do not expect an immediate
  // sync because we just tried (duh).
  scheduler.nextSync = Date.now() - 100000;
  scheduler.globalScore = SINGLE_USER_THRESHOLD + 1;
  function onSyncStart() {
    do_throw("Shouldn't have started a sync!");
  }
  Svc.Obs.add("weave:service:sync:start", onSyncStart);

  // Sanity check.
  do_check_eq(scheduler.syncTimer, null);
  do_check_eq(Status.checkSetup(), STATUS_OK);
  do_check_eq(Status.login, LOGIN_SUCCEEDED);

  scheduler.scheduleNextSync(0);
  yield deferred.promise;
});

add_identity_test(this, function test_loginError_fatal_clearsTriggers() {
  _("Verify that a fatal login error clears sync triggers.");
  yield configureIdentity({username: "johndoe"});

  let server = httpd_setup({
    "/1.1/johndoe/info/collections": httpd_handler(401, "Unauthorized")
  });

  Service.serverURL = server.baseURI + "/";
  Service.clusterURL = Service.serverURL;
  Service.persistLogin();
  Status.resetSync(); // reset Status.login

  let deferred = Promise.defer();
  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    Utils.nextTick(function aLittleBitAfterLoginError() {
      do_check_eq(Status.login, LOGIN_FAILED_LOGIN_REJECTED);

      do_check_eq(scheduler.nextSync, 0);
      do_check_eq(scheduler.syncTimer, null);

      cleanUpAndGo(server).then(deferred.resolve);
    });
  });

  // Sanity check.
  do_check_eq(scheduler.nextSync, 0);
  do_check_eq(scheduler.syncTimer, null);
  do_check_eq(Status.checkSetup(), STATUS_OK);
  do_check_eq(Status.login, LOGIN_SUCCEEDED);

  scheduler.scheduleNextSync(0);
  yield deferred.promise;
});

add_identity_test(this, function test_proper_interval_on_only_failing() {
  _("Ensure proper behavior when only failed records are applied.");

  // If an engine reports that no records succeeded, we shouldn't decrease the
  // sync interval.
  do_check_false(scheduler.hasIncomingItems);
  const INTERVAL = 10000000;
  scheduler.syncInterval = INTERVAL;

  Svc.Obs.notify("weave:service:sync:applied", {
    applied: 2,
    succeeded: 0,
    failed: 2,
    newFailed: 2,
    reconciled: 0
  });

  let deferred = Promise.defer();
  Utils.nextTick(function() {
    scheduler.adjustSyncInterval();
    do_check_false(scheduler.hasIncomingItems);
    do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

    deferred.resolve();
  });
  yield deferred.promise;
});
