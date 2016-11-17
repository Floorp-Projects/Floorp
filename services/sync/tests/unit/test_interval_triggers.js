/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");

var scheduler = Service.scheduler;
var clientsEngine = Service.clientsEngine;

// Don't remove stale clients when syncing. This is a test-only workaround
// that lets us add clients directly to the store, without losing them on
// the next sync.
clientsEngine._removeRemoteClient = id => {};

function promiseStopServer(server) {
  let deferred = Promise.defer();
  server.stop(deferred.resolve);
  return deferred.promise;
}

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
    "/1.1/johndoe/storage/clients": upd("clients", clientsColl.handler())
  });
}

function* setUp(server) {
  yield configureIdentity({username: "johndoe"});
  Service.serverURL = server.baseURI + "/";
  Service.clusterURL = server.baseURI + "/";
  generateNewKeys(Service.collectionKeys);
  let serverKeys = Service.collectionKeys.asWBO("crypto", "keys");
  serverKeys.encrypt(Service.identity.syncKeyBundle);
  serverKeys.upload(Service.resource(Service.cryptoKeysURL));
}

function run_test() {
  initTestLogging("Trace");

  Log.repository.getLogger("Sync.Service").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.SyncScheduler").level = Log.Level.Trace;

  run_next_test();
}

add_identity_test(this, function* test_successful_sync_adjustSyncInterval() {
  _("Test successful sync calling adjustSyncInterval");
  let syncSuccesses = 0;
  function onSyncFinish() {
    _("Sync success.");
    syncSuccesses++;
  };
  Svc.Obs.add("weave:service:sync:finish", onSyncFinish);

  let server = sync_httpd_setup();
  yield setUp(server);

  // Confirm defaults
  do_check_false(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
  do_check_false(scheduler.hasIncomingItems);

  _("Test as long as numClients <= 1 our sync interval is SINGLE_USER.");
  // idle == true && numClients <= 1 && hasIncomingItems == false
  scheduler.idle = true;
  Service.sync();
  do_check_eq(syncSuccesses, 1);
  do_check_true(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  // idle == false && numClients <= 1 && hasIncomingItems == false
  scheduler.idle = false;
  Service.sync();
  do_check_eq(syncSuccesses, 2);
  do_check_false(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  // idle == false && numClients <= 1 && hasIncomingItems == true
  scheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncSuccesses, 3);
  do_check_false(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_true(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  // idle == true && numClients <= 1 && hasIncomingItems == true
  scheduler.idle = true;
  Service.sync();
  do_check_eq(syncSuccesses, 4);
  do_check_true(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_true(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  _("Test as long as idle && numClients > 1 our sync interval is idleInterval.");
  // idle == true && numClients > 1 && hasIncomingItems == true
  Service.clientsEngine._store.create({id: "foo", cleartext: "bar"});
  Service.sync();
  do_check_eq(syncSuccesses, 5);
  do_check_true(scheduler.idle);
  do_check_true(scheduler.numClients > 1);
  do_check_true(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.idleInterval);

  // idle == true && numClients > 1 && hasIncomingItems == false
  scheduler.hasIncomingItems = false;
  Service.sync();
  do_check_eq(syncSuccesses, 6);
  do_check_true(scheduler.idle);
  do_check_true(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.idleInterval);

  _("Test non-idle, numClients > 1, no incoming items => activeInterval.");
  // idle == false && numClients > 1 && hasIncomingItems == false
  scheduler.idle = false;
  Service.sync();
  do_check_eq(syncSuccesses, 7);
  do_check_false(scheduler.idle);
  do_check_true(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);

  _("Test non-idle, numClients > 1, incoming items => immediateInterval.");
  // idle == false && numClients > 1 && hasIncomingItems == true
  scheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncSuccesses, 8);
  do_check_false(scheduler.idle);
  do_check_true(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems); //gets reset to false
  do_check_eq(scheduler.syncInterval, scheduler.immediateInterval);

  Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
  Service.startOver();
  yield promiseStopServer(server);
});

add_identity_test(this, function* test_unsuccessful_sync_adjustSyncInterval() {
  _("Test unsuccessful sync calling adjustSyncInterval");

  let syncFailures = 0;
  function onSyncError() {
    _("Sync error.");
    syncFailures++;
  }
  Svc.Obs.add("weave:service:sync:error", onSyncError);

  _("Test unsuccessful sync calls adjustSyncInterval");
  // Force sync to fail.
  Svc.Prefs.set("firstSync", "notReady");

  let server = sync_httpd_setup();
  yield setUp(server);

  // Confirm defaults
  do_check_false(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
  do_check_false(scheduler.hasIncomingItems);

  _("Test as long as numClients <= 1 our sync interval is SINGLE_USER.");
  // idle == true && numClients <= 1 && hasIncomingItems == false
  scheduler.idle = true;
  Service.sync();
  do_check_eq(syncFailures, 1);
  do_check_true(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  // idle == false && numClients <= 1 && hasIncomingItems == false
  scheduler.idle = false;
  Service.sync();
  do_check_eq(syncFailures, 2);
  do_check_false(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  // idle == false && numClients <= 1 && hasIncomingItems == true
  scheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncFailures, 3);
  do_check_false(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_true(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  // idle == true && numClients <= 1 && hasIncomingItems == true
  scheduler.idle = true;
  Service.sync();
  do_check_eq(syncFailures, 4);
  do_check_true(scheduler.idle);
  do_check_false(scheduler.numClients > 1);
  do_check_true(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  _("Test as long as idle && numClients > 1 our sync interval is idleInterval.");
  // idle == true && numClients > 1 && hasIncomingItems == true
  Service.clientsEngine._store.create({id: "foo", cleartext: "bar"});

  Service.sync();
  do_check_eq(syncFailures, 5);
  do_check_true(scheduler.idle);
  do_check_true(scheduler.numClients > 1);
  do_check_true(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.idleInterval);

  // idle == true && numClients > 1 && hasIncomingItems == false
  scheduler.hasIncomingItems = false;
  Service.sync();
  do_check_eq(syncFailures, 6);
  do_check_true(scheduler.idle);
  do_check_true(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.idleInterval);

  _("Test non-idle, numClients > 1, no incoming items => activeInterval.");
  // idle == false && numClients > 1 && hasIncomingItems == false
  scheduler.idle = false;
  Service.sync();
  do_check_eq(syncFailures, 7);
  do_check_false(scheduler.idle);
  do_check_true(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems);
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);

  _("Test non-idle, numClients > 1, incoming items => immediateInterval.");
  // idle == false && numClients > 1 && hasIncomingItems == true
  scheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncFailures, 8);
  do_check_false(scheduler.idle);
  do_check_true(scheduler.numClients > 1);
  do_check_false(scheduler.hasIncomingItems); //gets reset to false
  do_check_eq(scheduler.syncInterval, scheduler.immediateInterval);

  Service.startOver();
  Svc.Obs.remove("weave:service:sync:error", onSyncError);
  yield promiseStopServer(server);
});

add_identity_test(this, function* test_back_triggers_sync() {
  let server = sync_httpd_setup();
  yield setUp(server);

  // Single device: no sync triggered.
  scheduler.idle = true;
  scheduler.observe(null, "active", Svc.Prefs.get("scheduler.idleTime"));
  do_check_false(scheduler.idle);

  // Multiple devices: sync is triggered.
  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  scheduler.updateClientMode();

  let deferred = Promise.defer();
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);

    Service.recordManager.clearCache();
    Svc.Prefs.resetBranch("");
    scheduler.setDefaults();
    clientsEngine.resetClient();

    Service.startOver();
    server.stop(deferred.resolve);
  });

  scheduler.idle = true;
  scheduler.observe(null, "active", Svc.Prefs.get("scheduler.idleTime"));
  do_check_false(scheduler.idle);
  yield deferred.promise;
});

add_identity_test(this, function* test_adjust_interval_on_sync_error() {
  let server = sync_httpd_setup();
  yield setUp(server);

  let syncFailures = 0;
  function onSyncError() {
    _("Sync error.");
    syncFailures++;
  }
  Svc.Obs.add("weave:service:sync:error", onSyncError);

  _("Test unsuccessful sync updates client mode & sync intervals");
  // Force a sync fail.
  Svc.Prefs.set("firstSync", "notReady");

  do_check_eq(syncFailures, 0);
  do_check_false(scheduler.numClients > 1);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  Service.sync();

  do_check_eq(syncFailures, 1);
  do_check_true(scheduler.numClients > 1);
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);

  Svc.Obs.remove("weave:service:sync:error", onSyncError);
  Service.startOver();
  yield promiseStopServer(server);
});

add_identity_test(this, function* test_bug671378_scenario() {
  // Test scenario similar to bug 671378. This bug appeared when a score
  // update occurred that wasn't large enough to trigger a sync so
  // scheduleNextSync() was called without a time interval parameter,
  // setting nextSync to a non-zero value and preventing the timer from
  // being adjusted in the next call to scheduleNextSync().
  let server = sync_httpd_setup();
  yield setUp(server);

  let syncSuccesses = 0;
  function onSyncFinish() {
    _("Sync success.");
    syncSuccesses++;
  };
  Svc.Obs.add("weave:service:sync:finish", onSyncFinish);

  // After first sync call, syncInterval & syncTimer are singleDeviceInterval.
  Service.sync();
  do_check_eq(syncSuccesses, 1);
  do_check_false(scheduler.numClients > 1);
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
  do_check_eq(scheduler.syncTimer.delay, scheduler.singleDeviceInterval);

  let deferred = Promise.defer();
  // Wrap scheduleNextSync so we are notified when it is finished.
  scheduler._scheduleNextSync = scheduler.scheduleNextSync;
  scheduler.scheduleNextSync = function() {
    scheduler._scheduleNextSync();

    // Check on sync:finish scheduleNextSync sets the appropriate
    // syncInterval and syncTimer values.
    if (syncSuccesses == 2) {
      do_check_neq(scheduler.nextSync, 0);
      do_check_eq(scheduler.syncInterval, scheduler.activeInterval);
      do_check_true(scheduler.syncTimer.delay <= scheduler.activeInterval);

      scheduler.scheduleNextSync = scheduler._scheduleNextSync;
      Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
      Service.startOver();
      server.stop(deferred.resolve);
    }
  };

  // Set nextSync != 0
  // syncInterval still hasn't been set by call to updateClientMode.
  // Explicitly trying to invoke scheduleNextSync during a sync
  // (to immitate a score update that isn't big enough to trigger a sync).
  Svc.Obs.add("weave:service:sync:start", function onSyncStart() {
    // Wait for other sync:start observers to be called so that
    // nextSync is set to 0.
    Utils.nextTick(function() {
      Svc.Obs.remove("weave:service:sync:start", onSyncStart);

      scheduler.scheduleNextSync();
      do_check_neq(scheduler.nextSync, 0);
      do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);
      do_check_eq(scheduler.syncTimer.delay, scheduler.singleDeviceInterval);
    });
  });

  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  Service.sync();
  yield deferred.promise;
});

add_test(function test_adjust_timer_larger_syncInterval() {
  _("Test syncInterval > current timout period && nextSync != 0, syncInterval is NOT used.");
  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  scheduler.updateClientMode();
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);

  scheduler.scheduleNextSync();

  // Ensure we have a small interval.
  do_check_neq(scheduler.nextSync, 0);
  do_check_eq(scheduler.syncTimer.delay, scheduler.activeInterval);

  // Make interval large again
  clientsEngine._wipeClient();
  scheduler.updateClientMode();
  do_check_eq(scheduler.syncInterval, scheduler.singleDeviceInterval);

  scheduler.scheduleNextSync();

  // Ensure timer delay remains as the small interval.
  do_check_neq(scheduler.nextSync, 0);
  do_check_true(scheduler.syncTimer.delay <= scheduler.activeInterval);

  //SyncSchedule.
  Service.startOver();
  run_next_test();
});

add_test(function test_adjust_timer_smaller_syncInterval() {
  _("Test current timout > syncInterval period && nextSync != 0, syncInterval is used.");
  scheduler.scheduleNextSync();

  // Ensure we have a large interval.
  do_check_neq(scheduler.nextSync, 0);
  do_check_eq(scheduler.syncTimer.delay, scheduler.singleDeviceInterval);

  // Make interval smaller
  clientsEngine._store.create({id: "foo", cleartext: "bar"});
  scheduler.updateClientMode();
  do_check_eq(scheduler.syncInterval, scheduler.activeInterval);

  scheduler.scheduleNextSync();

  // Ensure smaller timer delay is used.
  do_check_neq(scheduler.nextSync, 0);
  do_check_true(scheduler.syncTimer.delay <= scheduler.activeInterval);

  //SyncSchedule.
  Service.startOver();
  run_next_test();
});
