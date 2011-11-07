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

function CatapultEngine() {
  SyncEngine.call(this, "Catapult");
}
CatapultEngine.prototype = {
  __proto__: SyncEngine.prototype,
  exception: null, // tests fill this in
  _sync: function _sync() {
    throw this.exception;
  }
};

Engines.register(CatapultEngine);

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
    "/1.1/johndoe/storage/clients": upd("clients", clientsColl.handler()),
    "/user/1.0/johndoe/node/weave": httpd_handler(200, "OK", "null")
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
  return serverKeys.upload(Service.cryptoKeysURL).success;
}

function cleanUpAndGo(server) {
  Utils.nextTick(function () {
    Service.startOver();
    if (server) {
      server.stop(run_next_test);
    } else {
      run_next_test();
    }
  });
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

  _("The 'nextSync' attribute stores a millisecond timestamp rounded down to the nearest second.");
  do_check_eq(SyncScheduler.nextSync, 0);
  SyncScheduler.nextSync = TIMESTAMP1;
  do_check_eq(SyncScheduler.nextSync, Math.floor(TIMESTAMP1 / 1000) * 1000);

  _("'syncInterval' defaults to singleDeviceInterval.");
  do_check_eq(Svc.Prefs.get('syncInterval'), undefined);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  _("'syncInterval' corresponds to a preference setting.");
  SyncScheduler.syncInterval = INTERVAL;
  do_check_eq(SyncScheduler.syncInterval, INTERVAL);
  do_check_eq(Svc.Prefs.get('syncInterval'), INTERVAL);

  _("'syncThreshold' corresponds to preference, defaults to SINGLE_USER_THRESHOLD");
  do_check_eq(Svc.Prefs.get('syncThreshold'), undefined);
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  SyncScheduler.syncThreshold = THRESHOLD;
  do_check_eq(SyncScheduler.syncThreshold, THRESHOLD);

  _("'globalScore' corresponds to preference, defaults to zero.");
  do_check_eq(Svc.Prefs.get('globalScore'), 0);
  do_check_eq(SyncScheduler.globalScore, 0);
  SyncScheduler.globalScore = SCORE;
  do_check_eq(SyncScheduler.globalScore, SCORE);
  do_check_eq(Svc.Prefs.get('globalScore'), SCORE);

  _("Intervals correspond to default preferences.");
  do_check_eq(SyncScheduler.singleDeviceInterval,
              Svc.Prefs.get("scheduler.singleDeviceInterval") * 1000);
  do_check_eq(SyncScheduler.idleInterval,
              Svc.Prefs.get("scheduler.idleInterval") * 1000);
  do_check_eq(SyncScheduler.activeInterval,
              Svc.Prefs.get("scheduler.activeInterval") * 1000);
  do_check_eq(SyncScheduler.immediateInterval,
              Svc.Prefs.get("scheduler.immediateInterval") * 1000);

  _("Custom values for prefs will take effect after a restart.");
  Svc.Prefs.set("scheduler.singleDeviceInterval", 42);
  Svc.Prefs.set("scheduler.idleInterval", 23);
  Svc.Prefs.set("scheduler.activeInterval", 18);
  Svc.Prefs.set("scheduler.immediateInterval", 31415);
  SyncScheduler.setDefaults();
  do_check_eq(SyncScheduler.idleInterval, 23000);
  do_check_eq(SyncScheduler.singleDeviceInterval, 42000);
  do_check_eq(SyncScheduler.activeInterval, 18000);
  do_check_eq(SyncScheduler.immediateInterval, 31415000);

  Svc.Prefs.resetBranch("");
  SyncScheduler.setDefaults();
  run_next_test();
});

add_test(function test_updateClientMode() {
  _("Test updateClientMode adjusts scheduling attributes based on # of clients appropriately");
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  // Trigger a change in interval & threshold by adding a client.
  Clients._store.create({id: "foo", cleartext: "bar"});
  SyncScheduler.updateClientMode();

  do_check_eq(SyncScheduler.syncThreshold, MULTI_DEVICE_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  // Resets the number of clients to 0. 
  Clients.resetClient();
  SyncScheduler.updateClientMode();

  // Goes back to single user if # clients is 1.
  do_check_eq(SyncScheduler.numClients, 1);
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  cleanUpAndGo();
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

  cleanUpAndGo(server);
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

  cleanUpAndGo();
});

add_test(function test_scheduleNextSync_nowOrPast() {
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
    cleanUpAndGo(server);
  });

  let server = sync_httpd_setup();
  setUp();

  // We're late for a sync...
  SyncScheduler.scheduleNextSync(-1);
});

add_test(function test_scheduleNextSync_future_noBackoff() {
  _("scheduleNextSync() uses the current syncInterval if no interval is provided.");
  // Test backoffInterval is 0 as expected.
  do_check_eq(Status.backoffInterval, 0);

  _("Test setting sync interval when nextSync == 0");
  SyncScheduler.nextSync = 0;
  SyncScheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(SyncScheduler.nextSync - Date.now()
                <= SyncScheduler.syncInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, SyncScheduler.syncInterval);

  _("Test setting sync interval when nextSync != 0");
  SyncScheduler.nextSync = Date.now() + SyncScheduler.singleDeviceInterval;
  SyncScheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(SyncScheduler.nextSync - Date.now()
                <= SyncScheduler.syncInterval);
  do_check_true(SyncScheduler.syncTimer.delay <= SyncScheduler.syncInterval);

  _("Scheduling requests for intervals larger than the current one will be ignored.");
  // Request a sync at a longer interval. The sync that's already scheduled
  // for sooner takes precedence.
  let nextSync = SyncScheduler.nextSync;
  let timerDelay = SyncScheduler.syncTimer.delay;
  let requestedInterval = SyncScheduler.syncInterval * 10;
  SyncScheduler.scheduleNextSync(requestedInterval);
  do_check_eq(SyncScheduler.nextSync, nextSync);
  do_check_eq(SyncScheduler.syncTimer.delay, timerDelay);

  // We can schedule anything we want if there isn't a sync scheduled.
  SyncScheduler.nextSync = 0;
  SyncScheduler.scheduleNextSync(requestedInterval);
  do_check_true(SyncScheduler.nextSync <= Date.now() + requestedInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, requestedInterval);

  // Request a sync at the smallest possible interval (0 triggers now).
  SyncScheduler.scheduleNextSync(1);
  do_check_true(SyncScheduler.nextSync <= Date.now() + 1);
  do_check_eq(SyncScheduler.syncTimer.delay, 1);

  cleanUpAndGo();
});

add_test(function test_scheduleNextSync_future_backoff() {
 _("scheduleNextSync() will honour backoff in all scheduling requests.");
  // Let's take a backoff interval that's bigger than the default sync interval.
  const BACKOFF = 7337;
  Status.backoffInterval = SyncScheduler.syncInterval + BACKOFF;

  _("Test setting sync interval when nextSync == 0");
  SyncScheduler.nextSync = 0;
  SyncScheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(SyncScheduler.nextSync - Date.now()
                <= Status.backoffInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, Status.backoffInterval);

  _("Test setting sync interval when nextSync != 0");
  SyncScheduler.nextSync = Date.now() + SyncScheduler.singleDeviceInterval;
  SyncScheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(SyncScheduler.nextSync - Date.now()
                <= Status.backoffInterval);
  do_check_true(SyncScheduler.syncTimer.delay <= Status.backoffInterval);

  // Request a sync at a longer interval. The sync that's already scheduled
  // for sooner takes precedence.
  let nextSync = SyncScheduler.nextSync;
  let timerDelay = SyncScheduler.syncTimer.delay;
  let requestedInterval = SyncScheduler.syncInterval * 10;
  do_check_true(requestedInterval > Status.backoffInterval);
  SyncScheduler.scheduleNextSync(requestedInterval);
  do_check_eq(SyncScheduler.nextSync, nextSync);
  do_check_eq(SyncScheduler.syncTimer.delay, timerDelay);

  // We can schedule anything we want if there isn't a sync scheduled.
  SyncScheduler.nextSync = 0;
  SyncScheduler.scheduleNextSync(requestedInterval);
  do_check_true(SyncScheduler.nextSync <= Date.now() + requestedInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, requestedInterval);

  // Request a sync at the smallest possible interval (0 triggers now).
  SyncScheduler.scheduleNextSync(1);
  do_check_true(SyncScheduler.nextSync <= Date.now() + Status.backoffInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, Status.backoffInterval);

  cleanUpAndGo();
});

add_test(function test_handleSyncError() {
  let server = sync_httpd_setup();
  setUp();
 
  // Force sync to fail.
  Svc.Prefs.set("firstSync", "notReady");

  _("Ensure expected initial environment.");
  do_check_eq(SyncScheduler._syncErrors, 0);
  do_check_false(Status.enforceBackoff);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  do_check_eq(Status.backoffInterval, 0);

  // Trigger sync with an error several times & observe
  // functionality of handleSyncError()
  _("Test first error calls scheduleNextSync on default interval");
  Service.sync();
  do_check_true(SyncScheduler.nextSync <= Date.now() + SyncScheduler.singleDeviceInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, SyncScheduler.singleDeviceInterval);
  do_check_eq(SyncScheduler._syncErrors, 1);
  do_check_false(Status.enforceBackoff);
  SyncScheduler.syncTimer.clear();

  _("Test second error still calls scheduleNextSync on default interval");
  Service.sync();
  do_check_true(SyncScheduler.nextSync <= Date.now() + SyncScheduler.singleDeviceInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, SyncScheduler.singleDeviceInterval);
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

  cleanUpAndGo(server);
});

add_test(function test_client_sync_finish_updateClientMode() {
  let server = sync_httpd_setup();
  setUp();

  // Confirm defaults.
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  do_check_false(SyncScheduler.idle);

  // Trigger a change in interval & threshold by adding a client.
  Clients._store.create({id: "foo", cleartext: "bar"});
  do_check_false(SyncScheduler.numClients > 1);
  SyncScheduler.updateClientMode();
  Service.sync();

  do_check_eq(SyncScheduler.syncThreshold, MULTI_DEVICE_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  // Resets the number of clients to 0. 
  Clients.resetClient();
  Service.sync();

  // Goes back to single user if # clients is 1.
  do_check_eq(SyncScheduler.numClients, 1);
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.idle);

  cleanUpAndGo(server);
});

add_test(function test_autoconnect_nextSync_past() {
  // nextSync will be 0 by default, so it's way in the past.

  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
    cleanUpAndGo(server);
  });

  let server = sync_httpd_setup();
  setUp();

  SyncScheduler.delayedAutoConnect(0);
});

add_test(function test_autoconnect_nextSync_future() {
  let previousSync = Date.now() + SyncScheduler.syncInterval / 2;
  SyncScheduler.nextSync = previousSync;
  // nextSync rounds to the nearest second.
  let expectedSync = SyncScheduler.nextSync;
  let expectedInterval = expectedSync - Date.now() - 1000;

  // Ensure we don't actually try to sync (or log in for that matter).
  function onLoginStart() {
    do_throw("Should not get here!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  waitForZeroTimer(function () {
    do_check_eq(SyncScheduler.nextSync, expectedSync);
    do_check_true(SyncScheduler.syncTimer.delay >= expectedInterval);

    Svc.Obs.remove("weave:service:login:start", onLoginStart);
    cleanUpAndGo();
  });

  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "abcdeabcdeabcdeabcdeabcdea";
  SyncScheduler.delayedAutoConnect(0);
});

add_test(function test_autoconnect_mp_locked() {
  let server = sync_httpd_setup();
  setUp();

  // Pretend user did not unlock master password.
  let origLocked = Utils.mpLocked;
  Utils.mpLocked = function() true;

  let origPP = Service.__lookupGetter__("passphrase");
  delete Service.passphrase;
  Service.__defineGetter__("passphrase", function() {
    _("Faking Master Password entry cancelation.");
    throw "User canceled Master Password entry";
  });

  // A locked master password will still trigger a sync, but then we'll hit
  // MASTER_PASSWORD_LOCKED and hence MASTER_PASSWORD_LOCKED_RETRY_INTERVAL.
  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    Utils.nextTick(function aLittleBitAfterLoginError() {
      do_check_eq(Status.login, MASTER_PASSWORD_LOCKED);

      Utils.mpLocked = origLocked;
      delete Service.passphrase;
      Service.__defineGetter__("passphrase", origPP);

      cleanUpAndGo(server);
    });
  });

  SyncScheduler.delayedAutoConnect(0);
});

add_test(function test_no_autoconnect_during_wizard() {
  let server = sync_httpd_setup();
  setUp();

  // Simulate the Sync setup wizard.
  Svc.Prefs.set("firstSync", "notReady");

  // Ensure we don't actually try to sync (or log in for that matter).
  function onLoginStart() {
    do_throw("Should not get here!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  waitForZeroTimer(function () {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);
    cleanUpAndGo(server);
  });

  SyncScheduler.delayedAutoConnect(0);
});

add_test(function test_no_autoconnect_status_not_ok() {
  let server = sync_httpd_setup();

  // Ensure we don't actually try to sync (or log in for that matter).
  function onLoginStart() {
    do_throw("Should not get here!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  waitForZeroTimer(function () {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);

    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);

    cleanUpAndGo(server);
  });

  SyncScheduler.delayedAutoConnect(0);
});

add_test(function test_autoconnectDelay_pref() {
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
    cleanUpAndGo(server);
  });

  Svc.Prefs.set("autoconnectDelay", 1);

  let server = sync_httpd_setup();
  setUp();

  Svc.Obs.notify("weave:service:ready");

  // autoconnectDelay pref is multiplied by 1000.
  do_check_eq(SyncScheduler._autoTimer.delay, 1000);
  do_check_eq(Status.service, STATUS_OK);
});

add_test(function test_idle_adjustSyncInterval() {
  // Confirm defaults.
  do_check_eq(SyncScheduler.idle, false);

  // Single device: nothing changes.
  SyncScheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_eq(SyncScheduler.idle, true);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  // Multiple devices: switch to idle interval.
  SyncScheduler.idle = false;
  Clients._store.create({id: "foo", cleartext: "bar"});
  SyncScheduler.updateClientMode();
  SyncScheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_eq(SyncScheduler.idle, true);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.idleInterval);

  cleanUpAndGo();
});

add_test(function test_back_triggersSync() {
  // Confirm defaults.
  do_check_false(SyncScheduler.idle);
  do_check_eq(Status.backoffInterval, 0);

  // Set up: Define 2 clients and put the system in idle.
  SyncScheduler.numClients = 2;
  SyncScheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_true(SyncScheduler.idle);

  // We don't actually expect the sync (or the login, for that matter) to
  // succeed. We just want to ensure that it was attempted.
  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    cleanUpAndGo();
  });

  // Send a 'back' event to trigger sync soonish.
  SyncScheduler.observe(null, "back", Svc.Prefs.get("scheduler.idleTime"));
});

add_test(function test_back_triggersSync_observesBackoff() {
  // Confirm defaults.
  do_check_false(SyncScheduler.idle);

  // Set up: Set backoff, define 2 clients and put the system in idle.
  const BACKOFF = 7337;
  Status.backoffInterval = SyncScheduler.idleInterval + BACKOFF;
  SyncScheduler.numClients = 2;
  SyncScheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_eq(SyncScheduler.idle, true);

  function onLoginStart() {
    do_throw("Shouldn't have kicked off a sync!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  timer = Utils.namedTimer(function () {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);

    do_check_true(SyncScheduler.nextSync <= Date.now() + Status.backoffInterval);
    do_check_eq(SyncScheduler.syncTimer.delay, Status.backoffInterval);

    cleanUpAndGo();
  }, IDLE_OBSERVER_BACK_DELAY * 1.5, {}, "timer");

  // Send a 'back' event to try to trigger sync soonish.
  SyncScheduler.observe(null, "back", Svc.Prefs.get("scheduler.idleTime"));
});

add_test(function test_back_debouncing() {
  _("Ensure spurious back-then-idle events, as observed on OS X, don't trigger a sync.");

  // Confirm defaults.
  do_check_eq(SyncScheduler.idle, false);

  // Set up: Define 2 clients and put the system in idle.
  SyncScheduler.numClients = 2;
  SyncScheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));
  do_check_eq(SyncScheduler.idle, true);

  function onLoginStart() {
    do_throw("Shouldn't have kicked off a sync!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  // Create spurious back-then-idle events as observed on OS X:
  SyncScheduler.observe(null, "back", Svc.Prefs.get("scheduler.idleTime"));
  SyncScheduler.observe(null, "idle", Svc.Prefs.get("scheduler.idleTime"));

  timer = Utils.namedTimer(function () {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);
    cleanUpAndGo();
  }, IDLE_OBSERVER_BACK_DELAY * 1.5, {}, "timer");
});

add_test(function test_no_sync_node() {
  // Test when Status.sync == NO_SYNC_NODE_FOUND
  // it is not overwritten on sync:finish
  let server = sync_httpd_setup();
  setUp();

  Service.serverURL = "http://localhost:8080/";

  Service.sync();
  do_check_eq(Status.sync, NO_SYNC_NODE_FOUND);
  do_check_eq(SyncScheduler.syncTimer.delay, NO_SYNC_NODE_INTERVAL);

  cleanUpAndGo(server);
});

add_test(function test_sync_failed_partial_500s() {
  _("Test a 5xx status calls handleSyncError.");
  SyncScheduler._syncErrors = MAX_ERROR_COUNT_BEFORE_BACKOFF;
  let server = sync_httpd_setup();

  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 500};

  do_check_eq(Status.sync, SYNC_SUCCEEDED);

  do_check_true(setUp());

  Service.sync();

  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);

  let maxInterval = SyncScheduler._syncErrors * (2 * MINIMUM_BACKOFF_INTERVAL);
  do_check_eq(Status.backoffInterval, 0);
  do_check_true(Status.enforceBackoff);
  do_check_eq(SyncScheduler._syncErrors, 4);
  do_check_true(SyncScheduler.nextSync <= (Date.now() + maxInterval));
  do_check_true(SyncScheduler.syncTimer.delay <= maxInterval);

  cleanUpAndGo(server);
});

add_test(function test_sync_failed_partial_400s() {
  _("Test a non-5xx status doesn't call handleSyncError.");
  SyncScheduler._syncErrors = MAX_ERROR_COUNT_BEFORE_BACKOFF;
  let server = sync_httpd_setup();

  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 400};

  // Have multiple devices for an active interval.
  Clients._store.create({id: "foo", cleartext: "bar"});

  do_check_eq(Status.sync, SYNC_SUCCEEDED);

  do_check_true(setUp());

  Service.sync();

  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);

  do_check_eq(Status.backoffInterval, 0);
  do_check_false(Status.enforceBackoff);
  do_check_eq(SyncScheduler._syncErrors, 0);
  do_check_true(SyncScheduler.nextSync <= (Date.now() + SyncScheduler.activeInterval));
  do_check_true(SyncScheduler.syncTimer.delay <= SyncScheduler.activeInterval);

  cleanUpAndGo(server);
});

add_test(function test_sync_X_Weave_Backoff() {
  let server = sync_httpd_setup();
  setUp();

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
  Clients._store.create({id: "foo", cleartext: "bar"});
  let rec = Clients._store.createRecord("foo", "clients");
  rec.encrypt();
  rec.upload(Clients.engineURL + rec.id);

  // Sync once to log in and get everything set up. Let's verify our initial
  // values.
  Service.sync();
  do_check_eq(Status.backoffInterval, 0);
  do_check_eq(Status.minimumNextSync, 0);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);
  do_check_true(SyncScheduler.nextSync <=
                Date.now() + SyncScheduler.syncInterval);
  // Sanity check that we picked the right value for BACKOFF:
  do_check_true(SyncScheduler.syncInterval < BACKOFF * 1000);

  // Turn on server maintenance and sync again.
  serverBackoff = true;
  Service.sync();

  do_check_true(Status.backoffInterval >= BACKOFF * 1000);
  // Allowing 1 second worth of of leeway between when Status.minimumNextSync
  // was set and when this line gets executed.
  let minimumExpectedDelay = (BACKOFF - 1) * 1000;
  do_check_true(Status.minimumNextSync >= Date.now() + minimumExpectedDelay);

  // Verify that the next sync is actually going to wait that long.
  do_check_true(SyncScheduler.nextSync >= Date.now() + minimumExpectedDelay);
  do_check_true(SyncScheduler.syncTimer.delay >= minimumExpectedDelay);

  cleanUpAndGo(server);
});

add_test(function test_sync_503_Retry_After() {
  let server = sync_httpd_setup();
  setUp();

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
  Clients._store.create({id: "foo", cleartext: "bar"});
  let rec = Clients._store.createRecord("foo", "clients");
  rec.encrypt();
  rec.upload(Clients.engineURL + rec.id);

  // Sync once to log in and get everything set up. Let's verify our initial
  // values.
  Service.sync();
  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.backoffInterval, 0);
  do_check_eq(Status.minimumNextSync, 0);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);
  do_check_true(SyncScheduler.nextSync <=
                Date.now() + SyncScheduler.syncInterval);
  // Sanity check that we picked the right value for BACKOFF:
  do_check_true(SyncScheduler.syncInterval < BACKOFF * 1000);

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
  do_check_true(SyncScheduler.nextSync >= Date.now() + minimumExpectedDelay);
  do_check_true(SyncScheduler.syncTimer.delay >= minimumExpectedDelay);

  cleanUpAndGo(server);
});

add_test(function test_loginError_recoverable_reschedules() {
  _("Verify that a recoverable login error schedules a new sync.");
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "abcdeabcdeabcdeabcdeabcdea";
  Service.serverURL  = "http://localhost:8080/";
  Service.clusterURL = "http://localhost:8080/";
  Service.persistLogin();
  Status.resetSync(); // reset Status.login

  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    Utils.nextTick(function aLittleBitAfterLoginError() {
      do_check_eq(Status.login, LOGIN_FAILED_NETWORK_ERROR);

      let expectedNextSync = Date.now() + SyncScheduler.syncInterval;
      do_check_true(SyncScheduler.nextSync > Date.now());
      do_check_true(SyncScheduler.nextSync <= expectedNextSync);
      do_check_true(SyncScheduler.syncTimer.delay > 0);
      do_check_true(SyncScheduler.syncTimer.delay <= SyncScheduler.syncInterval);

      Svc.Obs.remove("weave:service:sync:start", onSyncStart);
      cleanUpAndGo();
    });
  });

  // Let's set it up so that a sync is overdue, both in terms of previously
  // scheduled syncs and the global score. We still do not expect an immediate
  // sync because we just tried (duh).
  SyncScheduler.nextSync = Date.now() - 100000;
  SyncScheduler.globalScore = SINGLE_USER_THRESHOLD + 1;
  function onSyncStart() {
    do_throw("Shouldn't have started a sync!");
  }
  Svc.Obs.add("weave:service:sync:start", onSyncStart);

  // Sanity check.
  do_check_eq(SyncScheduler.syncTimer, null);
  do_check_eq(Status.checkSetup(), STATUS_OK);
  do_check_eq(Status.login, LOGIN_SUCCEEDED);

  SyncScheduler.scheduleNextSync(0);
});

add_test(function test_loginError_fatal_clearsTriggers() {
  _("Verify that a fatal login error clears sync triggers.");
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "abcdeabcdeabcdeabcdeabcdea";
  Service.serverURL  = "http://localhost:8080/";
  Service.clusterURL = "http://localhost:8080/";
  Service.persistLogin();
  Status.resetSync(); // reset Status.login

  let server = httpd_setup({
    "/1.1/johndoe/info/collections": httpd_handler(401, "Unauthorized")
  });

  Svc.Obs.add("weave:service:login:error", function onLoginError() {
    Svc.Obs.remove("weave:service:login:error", onLoginError);
    Utils.nextTick(function aLittleBitAfterLoginError() {
      do_check_eq(Status.login, LOGIN_FAILED_LOGIN_REJECTED);

      do_check_eq(SyncScheduler.nextSync, 0);
      do_check_eq(SyncScheduler.syncTimer, null);

      cleanUpAndGo(server);
    });
  });

  // Sanity check.
  do_check_eq(SyncScheduler.nextSync, 0);
  do_check_eq(SyncScheduler.syncTimer, null);
  do_check_eq(Status.checkSetup(), STATUS_OK);
  do_check_eq(Status.login, LOGIN_SUCCEEDED);

  SyncScheduler.scheduleNextSync(0);
});
