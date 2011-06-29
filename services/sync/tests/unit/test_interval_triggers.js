/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/policies.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");

function sync_httpd_setup() {
  // Tracking info/collections.
  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;

  return httpd_setup({
    "/1.1/johndoe/storage/meta/global":
      upd("meta", (new ServerWBO("global", {})).handler()),
    "/1.1/johndoe/info/collections": collectionsHelper.handler,
    "/1.1/johndoe/storage/crypto/keys":
      upd("crypto", (new ServerWBO("keys")).handler()),
    "/1.1/johndoe/storage/clients": upd("clients", (new ServerCollection()).handler())
  });
}

function setUp() {
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "sekrit";
  Service.clusterURL = "http://localhost:8080/";
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
  Svc.Obs.add("weave:service:sync:finish", function onSyncFinish() {
    _("Sync success.");
    syncSuccesses++;
  });

  let server = sync_httpd_setup();
  setUp();

  // Confirm defaults
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  do_check_false(SyncScheduler.hasIncomingItems);

  _("Test as long as numClients <= 1 our sync interval is SINGLE_USER."); 
  // idle == true && numClients <= 1 && hasIncomingItems == false
  Service.sync();
  do_check_eq(syncSuccesses, 1);
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  
  // idle == false && numClients <= 1 && hasIncomingItems == false
  SyncScheduler.idle = false;
  Service.sync();
  do_check_eq(syncSuccesses, 2);
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);

  // idle == false && numClients <= 1 && hasIncomingItems == true
  SyncScheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncSuccesses, 3);
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);

  // idle == true && numClients <= 1 && hasIncomingItems == true
  SyncScheduler.idle = true;
  Service.sync();
  do_check_eq(syncSuccesses, 4);
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);

  _("Test as long as idle && numClients > 1 our sync interval is MULTI_DEVICE_IDLE.");
  // idle == true && numClients > 1 && hasIncomingItems == true
  Clients._store.create({id: "foo", cleartext: "bar"});
  Service.sync();
  do_check_eq(syncSuccesses, 5);
  do_check_true(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_IDLE_SYNC);

  // idle == true && numClients > 1 && hasIncomingItems == false
  SyncScheduler.hasIncomingItems = false;
  Service.sync();
  do_check_eq(syncSuccesses, 6);
  do_check_true(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_IDLE_SYNC);

  _("Test non-idle, numClients > 1, no incoming items => MULTI_DEVICE_ACTIVE.");
  // idle == false && numClients > 1 && hasIncomingItems == false
  SyncScheduler.idle = false;
  Service.sync();
  do_check_eq(syncSuccesses, 7);
  do_check_false(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_ACTIVE_SYNC);
  
  _("Test non-idle, numClients > 1, incoming items => MULTI_DEVICE_IMMEDIATE.");
  // idle == false && numClients > 1 && hasIncomingItems == true
  SyncScheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncSuccesses, 8);
  do_check_false(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems); //gets reset to false
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_IMMEDIATE_SYNC);

  Records.clearCache();
  Svc.Prefs.resetBranch("");
  Clients.resetClient();

  // Reset defaults
  SyncScheduler.idle = true;
  SyncScheduler.updateClientMode();
  SyncScheduler.syncInterval = SINGLE_USER_SYNC;
  SyncScheduler.hasIncomingItems = false;

  server.stop(run_next_test);
});

add_test(function test_unsuccessful_sync_adjustSyncInterval() {
  _("Test unsuccessful sync calling adjustSyncInterval");

  let syncFailures = 0;
  Svc.Obs.add("weave:service:sync:error", function onSyncError() {
    _("Sync error.");
    syncFailures++;
  });
    
  _("Test unsuccessful sync calls adjustSyncInterval");
  let origLockedSync = Service._lockedSync;
  Service._lockedSync = function () {
    // Force a sync fail.
    Service._loggedIn = false;
    origLockedSync.call(Service);
  };
  
  let server = sync_httpd_setup();
  setUp();

  // Confirm defaults
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  do_check_false(SyncScheduler.hasIncomingItems);

  _("Test as long as numClients <= 1 our sync interval is SINGLE_USER."); 
  // idle == true && numClients <= 1 && hasIncomingItems == false
  Service.sync();
  do_check_eq(syncFailures, 1);
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  
  // idle == false && numClients <= 1 && hasIncomingItems == false
  SyncScheduler.idle = false;
  Service.sync();
  do_check_eq(syncFailures, 2);
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);

  // idle == false && numClients <= 1 && hasIncomingItems == true
  SyncScheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncFailures, 3);
  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);

  // idle == true && numClients <= 1 && hasIncomingItems == true
  SyncScheduler.idle = true;
  Service.sync();
  do_check_eq(syncFailures, 4);
  do_check_true(SyncScheduler.idle);
  do_check_false(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  
  _("Test as long as idle && numClients > 1 our sync interval is MULTI_DEVICE_IDLE.");
  // idle == true && numClients > 1 && hasIncomingItems == true
  
  // Doesn't get called if there is a sync error since clients
  // will not sync. So instead calling it here to force numClients > 1
  Clients._store.create({id: "foo", cleartext: "bar"});
  SyncScheduler.updateClientMode();  

  Service.sync();
  do_check_eq(syncFailures, 5);
  do_check_true(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_true(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_IDLE_SYNC);

  // idle == true && numClients > 1 && hasIncomingItems == false
  SyncScheduler.hasIncomingItems = false;
  Service.sync();
  do_check_eq(syncFailures, 6);
  do_check_true(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_IDLE_SYNC);

  _("Test non-idle, numClients > 1, no incoming items => MULTI_DEVICE_ACTIVE.");
  // idle == false && numClients > 1 && hasIncomingItems == false
  SyncScheduler.idle = false;
  Service.sync();
  do_check_eq(syncFailures, 7);
  do_check_false(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_ACTIVE_SYNC);
  
  _("Test non-idle, numClients > 1, incoming items => MULTI_DEVICE_IMMEDIATE.");
  // idle == false && numClients > 1 && hasIncomingItems == true
  SyncScheduler.hasIncomingItems = true;
  Service.sync();
  do_check_eq(syncFailures, 8);
  do_check_false(SyncScheduler.idle);
  do_check_true(SyncScheduler.numClients > 1);
  do_check_false(SyncScheduler.hasIncomingItems); //gets reset to false
  do_check_eq(SyncScheduler.syncInterval, MULTI_DEVICE_IMMEDIATE_SYNC);

  Records.clearCache();
  Svc.Prefs.resetBranch("");
  Clients.resetClient();
  Service._lockedSync = origLockedSync;

  // Reset defaults
  SyncScheduler.idle = true;
  SyncScheduler.syncInterval = SINGLE_USER_SYNC;
  SyncScheduler.hasIncomingItems = false;

  server.stop(run_next_test);
});
