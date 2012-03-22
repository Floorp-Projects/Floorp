/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/policies.js");

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
  setBasicCredentials("johndoe", "ilovejane", "abcdeabcdeabcdeabcdeabcdea");
  Service.serverURL = TEST_SERVER_URL;
  Service.clusterURL = TEST_CLUSTER_URL;

  generateNewKeys();
  let serverKeys = CollectionKeys.asWBO("crypto", "keys");
  serverKeys.encrypt(Identity.syncKeyBundle);
  return serverKeys.upload(Service.cryptoKeysURL);
}

function run_test() {
  initTestLogging("Trace");

  Log4Moz.repository.getLogger("Sync.Service").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.SyncScheduler").level = Log4Moz.Level.Trace;

  run_next_test();
}

add_test(function test_successful_sync_adjustSyncInterval() {
  _("Test successful sync calling adjustSyncInterval");
  let syncSuccesses = 0;
  function onSyncFinish() {
    _("Sync success.");
    syncSuccesses++;
  };
  Svc.Obs.add("weave:service:sync:finish", onSyncFinish);

  let server = sync_httpd_setup();
  setUp();

  // Confirm defaults
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  do_check_false(SyncScheduler.hasIncomingItems);

  _("Test as long as numClients <= 1 our sync interval is SINGLE_USER."); 
  // idle == true && numClients <= 1 && hasIncomingItems == false
  SyncScheduler.idle = true;
  Service.sync();
  do_check_eq(syncSuccesses, 1);
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  
  // idle == false && numClients <= 1 && hasIncomingItems == false
  SyncScheduler.idle = false;
  Service.sync();
  do_check_eq(syncSuccesses, 2);
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  // idle == false && numClients <= 1 && hasIncomingItems == true
  SyncScheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncSuccesses, 3);
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  // idle == true && numClients <= 1 && hasIncomingItems == true
  SyncScheduler.idle = true;
  Service.sync();
  do_check_eq(syncSuccesses, 4);
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  _("Test as long as idle && numClients > 1 our sync interval is idleInterval.");
  // idle == true && numClients > 1 && hasIncomingItems == true
  Clients._store.create({id: "foo", cleartext: "bar"});
  Service.sync();
  do_check_eq(syncSuccesses, 5);
  do_check_true(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.idleInterval);

  // idle == true && numClients > 1 && hasIncomingItems == false
  SyncScheduler.hasIncomingItems = false;
  Service.sync();
  do_check_eq(syncSuccesses, 6);
  do_check_true(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.idleInterval);

  _("Test non-idle, numClients > 1, no incoming items => activeInterval.");
  // idle == false && numClients > 1 && hasIncomingItems == false
  SyncScheduler.idle = false;
  Service.sync();
  do_check_eq(syncSuccesses, 7);
  do_check_false(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);
  
  _("Test non-idle, numClients > 1, incoming items => immediateInterval.");
  // idle == false && numClients > 1 && hasIncomingItems == true
  SyncScheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncSuccesses, 8);
  do_check_false(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems); //gets reset to false
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.immediateInterval);

  Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
  Service.startOver();
  server.stop(run_next_test);
});

add_test(function test_unsuccessful_sync_adjustSyncInterval() {
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
  setUp();

  // Confirm defaults
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  do_check_false(SyncScheduler.hasIncomingItems);

  _("Test as long as numClients <= 1 our sync interval is SINGLE_USER."); 
  // idle == true && numClients <= 1 && hasIncomingItems == false
  SyncScheduler.idle = true;
  Service.sync();
  do_check_eq(syncFailures, 1);
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  
  // idle == false && numClients <= 1 && hasIncomingItems == false
  SyncScheduler.idle = false;
  Service.sync();
  do_check_eq(syncFailures, 2);
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  // idle == false && numClients <= 1 && hasIncomingItems == true
  SyncScheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncFailures, 3);
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  // idle == true && numClients <= 1 && hasIncomingItems == true
  SyncScheduler.idle = true;
  Service.sync();
  do_check_eq(syncFailures, 4);
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  
  _("Test as long as idle && numClients > 1 our sync interval is idleInterval.");
  // idle == true && numClients > 1 && hasIncomingItems == true
  Clients._store.create({id: "foo", cleartext: "bar"});

  Service.sync();
  do_check_eq(syncFailures, 5);
  do_check_true(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.idleInterval);

  // idle == true && numClients > 1 && hasIncomingItems == false
  SyncScheduler.hasIncomingItems = false;
  Service.sync();
  do_check_eq(syncFailures, 6);
  do_check_true(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.idleInterval);

  _("Test non-idle, numClients > 1, no incoming items => activeInterval.");
  // idle == false && numClients > 1 && hasIncomingItems == false
  SyncScheduler.idle = false;
  Service.sync();
  do_check_eq(syncFailures, 7);
  do_check_false(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);
  
  _("Test non-idle, numClients > 1, incoming items => immediateInterval.");
  // idle == false && numClients > 1 && hasIncomingItems == true
  SyncScheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncFailures, 8);
  do_check_false(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems); //gets reset to false
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.immediateInterval);

  Service.startOver();
  Svc.Obs.remove("weave:service:sync:error", onSyncError);
  server.stop(run_next_test);
});

add_test(function test_back_triggers_sync() {
  let server = sync_httpd_setup();
  setUp();

  // Single device: no sync triggered.
  SyncScheduler.idle = true;
  SyncScheduler.observe(null, "back", Svc.Prefs.get("scheduler.idleTime"));
  do_check_false(SyncScheduler.idle);

  // Multiple devices: sync is triggered.
  Clients._store.create({id: "foo", cleartext: "bar"});
  SyncScheduler.updateClientMode();

  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);

    Records.clearCache();
    Svc.Prefs.resetBranch("");
    SyncScheduler.setDefaults();
    Clients.resetClient();

    Service.startOver();
    server.stop(run_next_test);
  });

  SyncScheduler.idle = true;
  SyncScheduler.observe(null, "back", Svc.Prefs.get("scheduler.idleTime"));
  do_check_false(SyncScheduler.idle);
});

add_test(function test_adjust_interval_on_sync_error() {
  let server = sync_httpd_setup();
  setUp();

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
  do_check_false(SyncScheduler.numClients > 1);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  Clients._store.create({id: "foo", cleartext: "bar"});
  Service.sync();

  do_check_eq(syncFailures, 1);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);

  Svc.Obs.remove("weave:service:sync:error", onSyncError);
  Service.startOver();
  server.stop(run_next_test);
});

add_test(function test_bug671378_scenario() {
  // Test scenario similar to bug 671378. This bug appeared when a score
  // update occurred that wasn't large enough to trigger a sync so 
  // scheduleNextSync() was called without a time interval parameter,
  // setting nextSync to a non-zero value and preventing the timer from
  // being adjusted in the next call to scheduleNextSync().
  let server = sync_httpd_setup();
  setUp();

  let syncSuccesses = 0;
  function onSyncFinish() {
    _("Sync success.");
    syncSuccesses++;
  };
  Svc.Obs.add("weave:service:sync:finish", onSyncFinish);

  // After first sync call, syncInterval & syncTimer are singleDeviceInterval.
  Service.sync();
  do_check_eq(syncSuccesses, 1);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
  do_check_eq(SyncScheduler.syncTimer.delay, SyncScheduler.singleDeviceInterval);

  // Wrap scheduleNextSync so we are notified when it is finished.
  SyncScheduler._scheduleNextSync = SyncScheduler.scheduleNextSync;
  SyncScheduler.scheduleNextSync = function() {
    SyncScheduler._scheduleNextSync();

    // Check on sync:finish scheduleNextSync sets the appropriate
    // syncInterval and syncTimer values.
    if (syncSuccesses == 2) {
      do_check_neq(SyncScheduler.nextSync, 0);
      do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);
      do_check_true(SyncScheduler.syncTimer.delay <= SyncScheduler.activeInterval);
 
      SyncScheduler.scheduleNextSync = SyncScheduler._scheduleNextSync;
      Svc.Obs.remove("weave:service:sync:finish", onSyncFinish);
      Service.startOver();
      server.stop(run_next_test);
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

      SyncScheduler.scheduleNextSync();
      do_check_neq(SyncScheduler.nextSync, 0);
      do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);
      do_check_eq(SyncScheduler.syncTimer.delay, SyncScheduler.singleDeviceInterval);
    });
  });

  Clients._store.create({id: "foo", cleartext: "bar"});
  Service.sync();
});

add_test(function test_adjust_timer_larger_syncInterval() {
  _("Test syncInterval > current timout period && nextSync != 0, syncInterval is NOT used.");
  Clients._store.create({id: "foo", cleartext: "bar"});
  SyncScheduler.updateClientMode();
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);

  SyncScheduler.scheduleNextSync();

  // Ensure we have a small interval.
  do_check_neq(SyncScheduler.nextSync, 0);
  do_check_eq(SyncScheduler.syncTimer.delay, SyncScheduler.activeInterval);

  // Make interval large again
  Clients._wipeClient();
  SyncScheduler.updateClientMode();
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  SyncScheduler.scheduleNextSync();

  // Ensure timer delay remains as the small interval.
  do_check_neq(SyncScheduler.nextSync, 0);
  do_check_true(SyncScheduler.syncTimer.delay <= SyncScheduler.activeInterval);

  //SyncSchedule.
  Service.startOver();
  run_next_test();
});

add_test(function test_adjust_timer_smaller_syncInterval() {
  _("Test current timout > syncInterval period && nextSync != 0, syncInterval is used.");
  SyncScheduler.scheduleNextSync();

  // Ensure we have a large interval.
  do_check_neq(SyncScheduler.nextSync, 0);
  do_check_eq(SyncScheduler.syncTimer.delay, SyncScheduler.singleDeviceInterval);

  // Make interval smaller
  Clients._store.create({id: "foo", cleartext: "bar"});
  SyncScheduler.updateClientMode();
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.activeInterval);

  SyncScheduler.scheduleNextSync();

  // Ensure smaller timer delay is used.
  do_check_neq(SyncScheduler.nextSync, 0);
  do_check_true(SyncScheduler.syncTimer.delay <= SyncScheduler.activeInterval);

  //SyncSchedule.
  Service.startOver();
  run_next_test();
});
