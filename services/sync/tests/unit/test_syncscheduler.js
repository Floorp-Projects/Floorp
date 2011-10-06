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

  _("'globalScore' corresponds to preference, defaults to zero.");
  do_check_eq(Svc.Prefs.get('globalScore'), undefined);
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

  Service.startOver();
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
  let server = sync_httpd_setup();
  setUp();

  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    // Ensure this gets called after SyncScheduler's observer so we
    // can cancel the timer set by SyncScheduler.scheduleNextSync().
    Utils.nextTick(function () {
      SyncScheduler.setDefaults();
      Svc.Prefs.resetBranch("");
      SyncScheduler.syncTimer.clear();
      Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
      Service.startOver();
      server.stop(run_next_test);
    }, this);
  });
  
  // Make sync happen faster
  SyncScheduler.singleDeviceInterval = 100;
  SyncScheduler.syncInterval = SyncScheduler.singleDeviceInterval;

  // Test backoffInterval is 0 as expected.
  do_check_eq(Status.backoffInterval, 0);

  _("Test setting sync interval when nextSync == 0");
  SyncScheduler.nextSync = 0;
  SyncScheduler.scheduleNextSync();

  // Test nextSync value was changed.
  do_check_true(SyncScheduler.nextSync > 0);

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  let expectedInterval = SyncScheduler.singleDeviceInterval;
  do_check_true(SyncScheduler.nextSync - Date.now() <= expectedInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, expectedInterval);

  _("Test setting sync interval when nextSync != 0");
  // Schedule next sync for 100ms in the future.
  SyncScheduler.nextSync = Date.now() + SyncScheduler.singleDeviceInterval;
  SyncScheduler.scheduleNextSync();

  // nextSync - Date.now() might be smaller than expectedInterval
  // since some time has passed since we called scheduleNextSync().
  do_check_true(SyncScheduler.nextSync - Date.now() <= expectedInterval);
  do_check_true(SyncScheduler.syncTimer.delay <= expectedInterval);
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

  Service.startOver();
  server.stop(run_next_test);
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

  Service.startOver();
  server.stop(run_next_test);
});

add_test(function test_autoconnect() {
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);

    Service.startOver();
    server.stop(run_next_test);
  });

  let server = sync_httpd_setup();
  setUp();

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

      Service.startOver();
      server.stop(run_next_test);
    });
  });

  SyncScheduler.delayedAutoConnect(0);
});

let timer;
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

  // First wait >100ms (nsITimers can take up to that much time to fire, so
  // we can account for the timer in delayedAutoconnect) and then two event
  // loop ticks (to account for the Utils.nextTick() in autoConnect).
  let ticks = 2;
  function wait() {
    if (ticks) {
      ticks -= 1;
      Utils.nextTick(wait);
      return;
    }
    Svc.Obs.remove("weave:service:login:start", onLoginStart);

    Service.startOver();
    server.stop(run_next_test);    
  }
  timer = Utils.namedTimer(wait, 150, {}, "timer");

  SyncScheduler.delayedAutoConnect(0);
});

add_test(function test_no_autoconnect_status_not_ok() {
  let server = sync_httpd_setup();

  // Ensure we don't actually try to sync (or log in for that matter).
  function onLoginStart() {
    do_throw("Should not get here!");
  }
  Svc.Obs.add("weave:service:login:start", onLoginStart);

  // First wait >100ms (nsITimers can take up to that much time to fire, so
  // we can account for the timer in delayedAutoconnect) and then two event
  // loop ticks (to account for the Utils.nextTick() in autoConnect).
  let ticks = 2;
  function wait() {
    if (ticks) {
      ticks -= 1;
      Utils.nextTick(wait);
      return;
    }
    Svc.Obs.remove("weave:service:login:start", onLoginStart);

    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Status.login, LOGIN_FAILED_NO_USERNAME);
    
    Service.startOver();
    server.stop(run_next_test); 
  }
  timer = Utils.namedTimer(wait, 150, {}, "timer");

  SyncScheduler.delayedAutoConnect(0);
});

add_test(function test_autoconnectDelay_pref() {
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);

    Service.startOver();
    server.stop(run_next_test);
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

  SyncScheduler.setDefaults();
  run_next_test();
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

  // Clean up.
  Service.startOver();
  Status.resetSync();
  server.stop(run_next_test);
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

  Status.resetSync();
  Service.startOver();
  server.stop(run_next_test);
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

  Status.resetSync();
  Service.startOver();
  server.stop(run_next_test);
});
