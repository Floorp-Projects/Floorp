/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/rotaryengine.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function makeRotaryEngine() {
  return new RotaryEngine(Service);
}

function clean(engine) {
  Svc.Prefs.resetBranch("");
  Svc.Prefs.set("log.logger.engine.rotary", "Trace");
  Service.recordManager.clearCache();
  engine._tracker.clearChangedIDs();
}

async function cleanAndGo(engine, server) {
  clean(engine);
  await promiseStopServer(server);
}

async function promiseClean(engine, server) {
  clean(engine);
  await promiseStopServer(server);
}

function configureService(server, username, password) {
  Service.clusterURL = server.baseURI;

  Service.identity.account = username || "foo";
  Service.identity.basicPassword = password || "password";
}

async function createServerAndConfigureClient() {
  let engine = new RotaryEngine(Service);

  let contents = {
    meta: {global: {engines: {rotary: {version: engine.version,
                                       syncID:  engine.syncID}}}},
    crypto: {},
    rotary: {}
  };

  const USER = "foo";
  let server = new SyncServer();
  server.registerUser(USER, "password");
  server.createContents(USER, contents);
  server.start();

  await SyncTestingInfrastructure(server, USER);
  Service._updateCachedURLs();

  return [engine, server, USER];
}

function run_test() {
  generateNewKeys(Service.collectionKeys);
  Svc.Prefs.set("log.logger.engine.rotary", "Trace");
  run_next_test();
}

/*
 * Tests
 *
 * SyncEngine._sync() is divided into four rather independent steps:
 *
 * - _syncStartup()
 * - _processIncoming()
 * - _uploadOutgoing()
 * - _syncFinish()
 *
 * In the spirit of unit testing, these are tested individually for
 * different scenarios below.
 */

add_task(async function test_syncStartup_emptyOrOutdatedGlobalsResetsSync() {
  _("SyncEngine._syncStartup resets sync and wipes server data if there's no or an outdated global record");

  // Some server side data that's going to be wiped
  let collection = new ServerCollection();
  collection.insert('flying',
                    encryptPayload({id: 'flying',
                                    denomination: "LNER Class A3 4472"}));
  collection.insert('scotsman',
                    encryptPayload({id: 'scotsman',
                                    denomination: "Flying Scotsman"}));

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  Service.identity.username = "foo";

  let engine = makeRotaryEngine();
  engine._store.items = {rekolok: "Rekonstruktionslokomotive"};
  try {

    // Confirm initial environment
    do_check_eq(engine._tracker.changedIDs["rekolok"], undefined);
    let metaGlobal = Service.recordManager.get(engine.metaURL);
    do_check_eq(metaGlobal.payload.engines, undefined);
    do_check_true(!!collection.payload("flying"));
    do_check_true(!!collection.payload("scotsman"));

    engine.lastSync = Date.now() / 1000;
    engine.lastSyncLocal = Date.now();

    // Trying to prompt a wipe -- we no longer track CryptoMeta per engine,
    // so it has nothing to check.
    engine._syncStartup();

    // The meta/global WBO has been filled with data about the engine
    let engineData = metaGlobal.payload.engines["rotary"];
    do_check_eq(engineData.version, engine.version);
    do_check_eq(engineData.syncID, engine.syncID);

    // Sync was reset and server data was wiped
    do_check_eq(engine.lastSync, 0);
    do_check_eq(collection.payload("flying"), undefined);
    do_check_eq(collection.payload("scotsman"), undefined);

  } finally {
    await cleanAndGo(engine, server);
  }
});

add_task(async function test_syncStartup_serverHasNewerVersion() {
  _("SyncEngine._syncStartup ");

  let global = new ServerWBO('global', {engines: {rotary: {version: 23456}}});
  let server = httpd_setup({
      "/1.1/foo/storage/meta/global": global.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  Service.identity.username = "foo";

  let engine = makeRotaryEngine();
  try {

    // The server has a newer version of the data and our engine can
    // handle.  That should give us an exception.
    let error;
    try {
      engine._syncStartup();
    } catch (ex) {
      error = ex;
    }
    do_check_eq(error.failureCode, VERSION_OUT_OF_DATE);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_syncStartup_syncIDMismatchResetsClient() {
  _("SyncEngine._syncStartup resets sync if syncIDs don't match");

  let server = sync_httpd_setup({});
  let syncTesting = await SyncTestingInfrastructure(server);
  Service.identity.username = "foo";

  // global record with a different syncID than our engine has
  let engine = makeRotaryEngine();
  let global = new ServerWBO('global',
                             {engines: {rotary: {version: engine.version,
                                                syncID: 'foobar'}}});
  server.registerPathHandler("/1.1/foo/storage/meta/global", global.handler());

  try {

    // Confirm initial environment
    do_check_eq(engine.syncID, 'fake-guid-00');
    do_check_eq(engine._tracker.changedIDs["rekolok"], undefined);

    engine.lastSync = Date.now() / 1000;
    engine.lastSyncLocal = Date.now();
    engine._syncStartup();

    // The engine has assumed the server's syncID
    do_check_eq(engine.syncID, 'foobar');

    // Sync was reset
    do_check_eq(engine.lastSync, 0);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_emptyServer() {
  _("SyncEngine._processIncoming working with an empty server backend");

  let collection = new ServerCollection();
  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  Service.identity.username = "foo";

  let engine = makeRotaryEngine();
  try {

    // Merely ensure that this code path is run without any errors
    engine._processIncoming();
    do_check_eq(engine.lastSync, 0);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_createFromServer() {
  _("SyncEngine._processIncoming creates new records from server data");

  // Some server records that will be downloaded
  let collection = new ServerCollection();
  collection.insert('flying',
                    encryptPayload({id: 'flying',
                                    denomination: "LNER Class A3 4472"}));
  collection.insert('scotsman',
                    encryptPayload({id: 'scotsman',
                                    denomination: "Flying Scotsman"}));

  // Two pathological cases involving relative URIs gone wrong.
  let pathologicalPayload = encryptPayload({id: '../pathological',
                                            denomination: "Pathological Case"});
  collection.insert('../pathological', pathologicalPayload);

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler(),
      "/1.1/foo/storage/rotary/flying": collection.wbo("flying").handler(),
      "/1.1/foo/storage/rotary/scotsman": collection.wbo("scotsman").handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  Service.identity.username = "foo";

  generateNewKeys(Service.collectionKeys);

  let engine = makeRotaryEngine();
  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    // Confirm initial environment
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.lastModified, null);
    do_check_eq(engine._store.items.flying, undefined);
    do_check_eq(engine._store.items.scotsman, undefined);
    do_check_eq(engine._store.items['../pathological'], undefined);

    engine._syncStartup();
    engine._processIncoming();

    // Timestamps of last sync and last server modification are set.
    do_check_true(engine.lastSync > 0);
    do_check_true(engine.lastModified > 0);

    // Local records have been created from the server data.
    do_check_eq(engine._store.items.flying, "LNER Class A3 4472");
    do_check_eq(engine._store.items.scotsman, "Flying Scotsman");
    do_check_eq(engine._store.items['../pathological'], "Pathological Case");

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_reconcile() {
  _("SyncEngine._processIncoming updates local records");

  let collection = new ServerCollection();

  // This server record is newer than the corresponding client one,
  // so it'll update its data.
  collection.insert('newrecord',
                    encryptPayload({id: 'newrecord',
                                    denomination: "New stuff..."}));

  // This server record is newer than the corresponding client one,
  // so it'll update its data.
  collection.insert('newerserver',
                    encryptPayload({id: 'newerserver',
                                    denomination: "New data!"}));

  // This server record is 2 mins older than the client counterpart
  // but identical to it, so we're expecting the client record's
  // changedID to be reset.
  collection.insert('olderidentical',
                    encryptPayload({id: 'olderidentical',
                                    denomination: "Older but identical"}));
  collection._wbos.olderidentical.modified -= 120;

  // This item simply has different data than the corresponding client
  // record (which is unmodified), so it will update the client as well
  collection.insert('updateclient',
                    encryptPayload({id: 'updateclient',
                                    denomination: "Get this!"}));

  // This is a dupe of 'original'.
  collection.insert('duplication',
                    encryptPayload({id: 'duplication',
                                    denomination: "Original Entry"}));

  // This record is marked as deleted, so we're expecting the client
  // record to be removed.
  collection.insert('nukeme',
                    encryptPayload({id: 'nukeme',
                                    denomination: "Nuke me!",
                                    deleted: true}));

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  Service.identity.username = "foo";

  let engine = makeRotaryEngine();
  engine._store.items = {newerserver: "New data, but not as new as server!",
                         olderidentical: "Older but identical",
                         updateclient: "Got data?",
                         original: "Original Entry",
                         long_original: "Long Original Entry",
                         nukeme: "Nuke me!"};
  // Make this record 1 min old, thus older than the one on the server
  engine._tracker.addChangedID('newerserver', Date.now() / 1000 - 60);
  // This record has been changed 2 mins later than the one on the server
  engine._tracker.addChangedID('olderidentical', Date.now() / 1000);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    // Confirm initial environment
    do_check_eq(engine._store.items.newrecord, undefined);
    do_check_eq(engine._store.items.newerserver, "New data, but not as new as server!");
    do_check_eq(engine._store.items.olderidentical, "Older but identical");
    do_check_eq(engine._store.items.updateclient, "Got data?");
    do_check_eq(engine._store.items.nukeme, "Nuke me!");
    do_check_true(engine._tracker.changedIDs['olderidentical'] > 0);

    engine._syncStartup();
    engine._processIncoming();

    // Timestamps of last sync and last server modification are set.
    do_check_true(engine.lastSync > 0);
    do_check_true(engine.lastModified > 0);

    // The new record is created.
    do_check_eq(engine._store.items.newrecord, "New stuff...");

    // The 'newerserver' record is updated since the server data is newer.
    do_check_eq(engine._store.items.newerserver, "New data!");

    // The data for 'olderidentical' is identical on the server, so
    // it's no longer marked as changed anymore.
    do_check_eq(engine._store.items.olderidentical, "Older but identical");
    do_check_eq(engine._tracker.changedIDs['olderidentical'], undefined);

    // Updated with server data.
    do_check_eq(engine._store.items.updateclient, "Get this!");

    // The incoming ID is preferred.
    do_check_eq(engine._store.items.original, undefined);
    do_check_eq(engine._store.items.duplication, "Original Entry");
    do_check_neq(engine._delete.ids.indexOf("original"), -1);

    // The 'nukeme' record marked as deleted is removed.
    do_check_eq(engine._store.items.nukeme, undefined);
  } finally {
    await cleanAndGo(engine, server);
  }
});

add_task(async function test_processIncoming_reconcile_local_deleted() {
  _("Ensure local, duplicate ID is deleted on server.");

  // When a duplicate is resolved, the local ID (which is never taken) should
  // be deleted on the server.
  let [engine, server, user] = await createServerAndConfigureClient();

  let now = Date.now() / 1000 - 10;
  engine.lastSync = now;
  engine.lastModified = now + 1;

  let record = encryptPayload({id: "DUPE_INCOMING", denomination: "incoming"});
  let wbo = new ServerWBO("DUPE_INCOMING", record, now + 2);
  server.insertWBO(user, "rotary", wbo);

  record = encryptPayload({id: "DUPE_LOCAL", denomination: "local"});
  wbo = new ServerWBO("DUPE_LOCAL", record, now - 1);
  server.insertWBO(user, "rotary", wbo);

  engine._store.create({id: "DUPE_LOCAL", denomination: "local"});
  do_check_true(engine._store.itemExists("DUPE_LOCAL"));
  do_check_eq("DUPE_LOCAL", engine._findDupe({id: "DUPE_INCOMING"}));

  engine._sync();

  do_check_attribute_count(engine._store.items, 1);
  do_check_true("DUPE_INCOMING" in engine._store.items);

  let collection = server.getCollection(user, "rotary");
  do_check_eq(1, collection.count());
  do_check_neq(undefined, collection.wbo("DUPE_INCOMING"));

  await cleanAndGo(engine, server);
});

add_task(async function test_processIncoming_reconcile_equivalent() {
  _("Ensure proper handling of incoming records that match local.");

  let [engine, server, user] = await createServerAndConfigureClient();

  let now = Date.now() / 1000 - 10;
  engine.lastSync = now;
  engine.lastModified = now + 1;

  let record = encryptPayload({id: "entry", denomination: "denomination"});
  let wbo = new ServerWBO("entry", record, now + 2);
  server.insertWBO(user, "rotary", wbo);

  engine._store.items = {entry: "denomination"};
  do_check_true(engine._store.itemExists("entry"));

  engine._sync();

  do_check_attribute_count(engine._store.items, 1);

  await cleanAndGo(engine, server);
});

add_task(async function test_processIncoming_reconcile_locally_deleted_dupe_new() {
  _("Ensure locally deleted duplicate record newer than incoming is handled.");

  // This is a somewhat complicated test. It ensures that if a client receives
  // a modified record for an item that is deleted locally but with a different
  // ID that the incoming record is ignored. This is a corner case for record
  // handling, but it needs to be supported.
  let [engine, server, user] = await createServerAndConfigureClient();

  let now = Date.now() / 1000 - 10;
  engine.lastSync = now;
  engine.lastModified = now + 1;

  let record = encryptPayload({id: "DUPE_INCOMING", denomination: "incoming"});
  let wbo = new ServerWBO("DUPE_INCOMING", record, now + 2);
  server.insertWBO(user, "rotary", wbo);

  // Simulate a locally-deleted item.
  engine._store.items = {};
  engine._tracker.addChangedID("DUPE_LOCAL", now + 3);
  do_check_false(engine._store.itemExists("DUPE_LOCAL"));
  do_check_false(engine._store.itemExists("DUPE_INCOMING"));
  do_check_eq("DUPE_LOCAL", engine._findDupe({id: "DUPE_INCOMING"}));

  engine._sync();

  // After the sync, the server's payload for the original ID should be marked
  // as deleted.
  do_check_empty(engine._store.items);
  let collection = server.getCollection(user, "rotary");
  do_check_eq(1, collection.count());
  wbo = collection.wbo("DUPE_INCOMING");
  do_check_neq(null, wbo);
  let payload = JSON.parse(JSON.parse(wbo.payload).ciphertext);
  do_check_true(payload.deleted);

  await cleanAndGo(engine, server);
});

add_task(async function test_processIncoming_reconcile_locally_deleted_dupe_old() {
  _("Ensure locally deleted duplicate record older than incoming is restored.");

  // This is similar to the above test except it tests the condition where the
  // incoming record is newer than the local deletion, therefore overriding it.

  let [engine, server, user] = await createServerAndConfigureClient();

  let now = Date.now() / 1000 - 10;
  engine.lastSync = now;
  engine.lastModified = now + 1;

  let record = encryptPayload({id: "DUPE_INCOMING", denomination: "incoming"});
  let wbo = new ServerWBO("DUPE_INCOMING", record, now + 2);
  server.insertWBO(user, "rotary", wbo);

  // Simulate a locally-deleted item.
  engine._store.items = {};
  engine._tracker.addChangedID("DUPE_LOCAL", now + 1);
  do_check_false(engine._store.itemExists("DUPE_LOCAL"));
  do_check_false(engine._store.itemExists("DUPE_INCOMING"));
  do_check_eq("DUPE_LOCAL", engine._findDupe({id: "DUPE_INCOMING"}));

  engine._sync();

  // Since the remote change is newer, the incoming item should exist locally.
  do_check_attribute_count(engine._store.items, 1);
  do_check_true("DUPE_INCOMING" in engine._store.items);
  do_check_eq("incoming", engine._store.items.DUPE_INCOMING);

  let collection = server.getCollection(user, "rotary");
  do_check_eq(1, collection.count());
  wbo = collection.wbo("DUPE_INCOMING");
  let payload = JSON.parse(JSON.parse(wbo.payload).ciphertext);
  do_check_eq("incoming", payload.denomination);

  await cleanAndGo(engine, server);
});

add_task(async function test_processIncoming_reconcile_changed_dupe() {
  _("Ensure that locally changed duplicate record is handled properly.");

  let [engine, server, user] = await createServerAndConfigureClient();

  let now = Date.now() / 1000 - 10;
  engine.lastSync = now;
  engine.lastModified = now + 1;

  // The local record is newer than the incoming one, so it should be retained.
  let record = encryptPayload({id: "DUPE_INCOMING", denomination: "incoming"});
  let wbo = new ServerWBO("DUPE_INCOMING", record, now + 2);
  server.insertWBO(user, "rotary", wbo);

  engine._store.create({id: "DUPE_LOCAL", denomination: "local"});
  engine._tracker.addChangedID("DUPE_LOCAL", now + 3);
  do_check_true(engine._store.itemExists("DUPE_LOCAL"));
  do_check_eq("DUPE_LOCAL", engine._findDupe({id: "DUPE_INCOMING"}));

  engine._sync();

  // The ID should have been changed to incoming.
  do_check_attribute_count(engine._store.items, 1);
  do_check_true("DUPE_INCOMING" in engine._store.items);

  // On the server, the local ID should be deleted and the incoming ID should
  // have its payload set to what was in the local record.
  let collection = server.getCollection(user, "rotary");
  do_check_eq(1, collection.count());
  wbo = collection.wbo("DUPE_INCOMING");
  do_check_neq(undefined, wbo);
  let payload = JSON.parse(JSON.parse(wbo.payload).ciphertext);
  do_check_eq("local", payload.denomination);

  await cleanAndGo(engine, server);
});

add_task(async function test_processIncoming_reconcile_changed_dupe_new() {
  _("Ensure locally changed duplicate record older than incoming is ignored.");

  // This test is similar to the above except the incoming record is younger
  // than the local record. The incoming record should be authoritative.
  let [engine, server, user] = await createServerAndConfigureClient();

  let now = Date.now() / 1000 - 10;
  engine.lastSync = now;
  engine.lastModified = now + 1;

  let record = encryptPayload({id: "DUPE_INCOMING", denomination: "incoming"});
  let wbo = new ServerWBO("DUPE_INCOMING", record, now + 2);
  server.insertWBO(user, "rotary", wbo);

  engine._store.create({id: "DUPE_LOCAL", denomination: "local"});
  engine._tracker.addChangedID("DUPE_LOCAL", now + 1);
  do_check_true(engine._store.itemExists("DUPE_LOCAL"));
  do_check_eq("DUPE_LOCAL", engine._findDupe({id: "DUPE_INCOMING"}));

  engine._sync();

  // The ID should have been changed to incoming.
  do_check_attribute_count(engine._store.items, 1);
  do_check_true("DUPE_INCOMING" in engine._store.items);

  // On the server, the local ID should be deleted and the incoming ID should
  // have its payload retained.
  let collection = server.getCollection(user, "rotary");
  do_check_eq(1, collection.count());
  wbo = collection.wbo("DUPE_INCOMING");
  do_check_neq(undefined, wbo);
  let payload = JSON.parse(JSON.parse(wbo.payload).ciphertext);
  do_check_eq("incoming", payload.denomination);
  await cleanAndGo(engine, server);
});

add_task(async function test_processIncoming_mobile_batchSize() {
  _("SyncEngine._processIncoming doesn't fetch everything at once on mobile clients");

  Svc.Prefs.set("client.type", "mobile");
  Service.identity.username = "foo";

  // A collection that logs each GET
  let collection = new ServerCollection();
  collection.get_log = [];
  collection._get = collection.get;
  collection.get = function(options) {
    this.get_log.push(options);
    return this._get(options);
  };

  // Let's create some 234 server side records. They're all at least
  // 10 minutes old.
  for (let i = 0; i < 234; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + i});
    let wbo = new ServerWBO(id, payload);
    wbo.modified = Date.now() / 1000 - 60 * (i + 10);
    collection.insertWBO(wbo);
  }

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let engine = makeRotaryEngine();
  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    _("On a mobile client, we get new records from the server in batches of 50.");
    engine._syncStartup();
    engine._processIncoming();
    do_check_attribute_count(engine._store.items, 234);
    do_check_true('record-no-0' in engine._store.items);
    do_check_true('record-no-49' in engine._store.items);
    do_check_true('record-no-50' in engine._store.items);
    do_check_true('record-no-233' in engine._store.items);

    // Verify that the right number of GET requests with the right
    // kind of parameters were made.
    do_check_eq(collection.get_log.length,
                Math.ceil(234 / MOBILE_BATCH_SIZE) + 1);
    do_check_eq(collection.get_log[0].full, 1);
    do_check_eq(collection.get_log[0].limit, MOBILE_BATCH_SIZE);
    do_check_eq(collection.get_log[1].full, undefined);
    do_check_eq(collection.get_log[1].limit, undefined);
    for (let i = 1; i <= Math.floor(234 / MOBILE_BATCH_SIZE); i++) {
      do_check_eq(collection.get_log[i + 1].full, 1);
      do_check_eq(collection.get_log[i + 1].limit, undefined);
      if (i < Math.floor(234 / MOBILE_BATCH_SIZE))
        do_check_eq(collection.get_log[i + 1].ids.length, MOBILE_BATCH_SIZE);
      else
        do_check_eq(collection.get_log[i + 1].ids.length, 234 % MOBILE_BATCH_SIZE);
    }

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_store_toFetch() {
  _("If processIncoming fails in the middle of a batch on mobile, state is saved in toFetch and lastSync.");
  Service.identity.username = "foo";
  Svc.Prefs.set("client.type", "mobile");

  // A collection that throws at the fourth get.
  let collection = new ServerCollection();
  collection._get_calls = 0;
  collection._get = collection.get;
  collection.get = function() {
    this._get_calls += 1;
    if (this._get_calls > 3) {
      throw "Abort on fourth call!";
    }
    return this._get.apply(this, arguments);
  };

  // Let's create three batches worth of server side records.
  for (var i = 0; i < MOBILE_BATCH_SIZE * 3; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + id});
    let wbo = new ServerWBO(id, payload);
    wbo.modified = Date.now() / 1000 + 60 * (i - MOBILE_BATCH_SIZE * 3);
    collection.insertWBO(wbo);
  }

  let engine = makeRotaryEngine();
  engine.enabled = true;

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};
  try {

    // Confirm initial environment
    do_check_eq(engine.lastSync, 0);
    do_check_empty(engine._store.items);

    let error;
    try {
      await sync_engine_and_validate_telem(engine, true);
    } catch (ex) {
      error = ex;
    }

    // Only the first two batches have been applied.
    do_check_eq(Object.keys(engine._store.items).length,
                MOBILE_BATCH_SIZE * 2);

    // The third batch is stuck in toFetch. lastSync has been moved forward to
    // the last successful item's timestamp.
    do_check_eq(engine.toFetch.length, MOBILE_BATCH_SIZE);
    do_check_eq(engine.lastSync, collection.wbo("record-no-99").modified);

  } finally {
    await promiseClean(engine, server);
  }
});


add_task(async function test_processIncoming_resume_toFetch() {
  _("toFetch and previousFailed items left over from previous syncs are fetched on the next sync, along with new items.");
  Service.identity.username = "foo";

  const LASTSYNC = Date.now() / 1000;

  // Server records that will be downloaded
  let collection = new ServerCollection();
  collection.insert('flying',
                    encryptPayload({id: 'flying',
                                    denomination: "LNER Class A3 4472"}));
  collection.insert('scotsman',
                    encryptPayload({id: 'scotsman',
                                    denomination: "Flying Scotsman"}));
  collection.insert('rekolok',
                    encryptPayload({id: 'rekolok',
                                    denomination: "Rekonstruktionslokomotive"}));
  for (let i = 0; i < 3; i++) {
    let id = 'failed' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + i});
    let wbo = new ServerWBO(id, payload);
    wbo.modified = LASTSYNC - 10;
    collection.insertWBO(wbo);
  }

  collection.wbo("flying").modified =
    collection.wbo("scotsman").modified = LASTSYNC - 10;
  collection._wbos.rekolok.modified = LASTSYNC + 10;

  // Time travel 10 seconds into the future but still download the above WBOs.
  let engine = makeRotaryEngine();
  engine.lastSync = LASTSYNC;
  engine.toFetch = ["flying", "scotsman"];
  engine.previousFailed = ["failed0", "failed1", "failed2"];

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};
  try {

    // Confirm initial environment
    do_check_eq(engine._store.items.flying, undefined);
    do_check_eq(engine._store.items.scotsman, undefined);
    do_check_eq(engine._store.items.rekolok, undefined);

    engine._syncStartup();
    engine._processIncoming();

    // Local records have been created from the server data.
    do_check_eq(engine._store.items.flying, "LNER Class A3 4472");
    do_check_eq(engine._store.items.scotsman, "Flying Scotsman");
    do_check_eq(engine._store.items.rekolok, "Rekonstruktionslokomotive");
    do_check_eq(engine._store.items.failed0, "Record No. 0");
    do_check_eq(engine._store.items.failed1, "Record No. 1");
    do_check_eq(engine._store.items.failed2, "Record No. 2");
    do_check_eq(engine.previousFailed.length, 0);
  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_applyIncomingBatchSize_smaller() {
  _("Ensure that a number of incoming items less than applyIncomingBatchSize is still applied.");
  Service.identity.username = "foo";

  // Engine that doesn't like the first and last record it's given.
  const APPLY_BATCH_SIZE = 10;
  let engine = makeRotaryEngine();
  engine.applyIncomingBatchSize = APPLY_BATCH_SIZE;
  engine._store._applyIncomingBatch = engine._store.applyIncomingBatch;
  engine._store.applyIncomingBatch = function(records) {
    let failed1 = records.shift();
    let failed2 = records.pop();
    this._applyIncomingBatch(records);
    return [failed1.id, failed2.id];
  };

  // Let's create less than a batch worth of server side records.
  let collection = new ServerCollection();
  for (let i = 0; i < APPLY_BATCH_SIZE - 1; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + id});
    collection.insert(id, payload);
  }

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};
  try {

    // Confirm initial environment
    do_check_empty(engine._store.items);

    engine._syncStartup();
    engine._processIncoming();

    // Records have been applied and the expected failures have failed.
    do_check_attribute_count(engine._store.items, APPLY_BATCH_SIZE - 1 - 2);
    do_check_eq(engine.toFetch.length, 0);
    do_check_eq(engine.previousFailed.length, 2);
    do_check_eq(engine.previousFailed[0], "record-no-0");
    do_check_eq(engine.previousFailed[1], "record-no-8");

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_applyIncomingBatchSize_multiple() {
  _("Ensure that incoming items are applied according to applyIncomingBatchSize.");
  Service.identity.username = "foo";

  const APPLY_BATCH_SIZE = 10;

  // Engine that applies records in batches.
  let engine = makeRotaryEngine();
  engine.applyIncomingBatchSize = APPLY_BATCH_SIZE;
  let batchCalls = 0;
  engine._store._applyIncomingBatch = engine._store.applyIncomingBatch;
  engine._store.applyIncomingBatch = function(records) {
    batchCalls += 1;
    do_check_eq(records.length, APPLY_BATCH_SIZE);
    this._applyIncomingBatch.apply(this, arguments);
  };

  // Let's create three batches worth of server side records.
  let collection = new ServerCollection();
  for (let i = 0; i < APPLY_BATCH_SIZE * 3; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + id});
    collection.insert(id, payload);
  }

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};
  try {

    // Confirm initial environment
    do_check_empty(engine._store.items);

    engine._syncStartup();
    engine._processIncoming();

    // Records have been applied in 3 batches.
    do_check_eq(batchCalls, 3);
    do_check_attribute_count(engine._store.items, APPLY_BATCH_SIZE * 3);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_notify_count() {
  _("Ensure that failed records are reported only once.");
  Service.identity.username = "foo";

  const APPLY_BATCH_SIZE = 5;
  const NUMBER_OF_RECORDS = 15;

  // Engine that fails the first record.
  let engine = makeRotaryEngine();
  engine.applyIncomingBatchSize = APPLY_BATCH_SIZE;
  engine._store._applyIncomingBatch = engine._store.applyIncomingBatch;
  engine._store.applyIncomingBatch = function(records) {
    engine._store._applyIncomingBatch(records.slice(1));
    return [records[0].id];
  };

  // Create a batch of server side records.
  let collection = new ServerCollection();
  for (var i = 0; i < NUMBER_OF_RECORDS; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + id});
    collection.insert(id, payload);
  }

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};
  try {
    // Confirm initial environment.
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.toFetch.length, 0);
    do_check_eq(engine.previousFailed.length, 0);
    do_check_empty(engine._store.items);

    let called = 0;
    let counts;
    function onApplied(count) {
      _("Called with " + JSON.stringify(counts));
      counts = count;
      called++;
    }
    Svc.Obs.add("weave:engine:sync:applied", onApplied);

    // Do sync.
    engine._syncStartup();
    engine._processIncoming();

    // Confirm failures.
    do_check_attribute_count(engine._store.items, 12);
    do_check_eq(engine.previousFailed.length, 3);
    do_check_eq(engine.previousFailed[0], "record-no-0");
    do_check_eq(engine.previousFailed[1], "record-no-5");
    do_check_eq(engine.previousFailed[2], "record-no-10");

    // There are newly failed records and they are reported.
    do_check_eq(called, 1);
    do_check_eq(counts.failed, 3);
    do_check_eq(counts.applied, 15);
    do_check_eq(counts.newFailed, 3);
    do_check_eq(counts.succeeded, 12);

    // Sync again, 1 of the failed items are the same, the rest didn't fail.
    engine._processIncoming();

    // Confirming removed failures.
    do_check_attribute_count(engine._store.items, 14);
    do_check_eq(engine.previousFailed.length, 1);
    do_check_eq(engine.previousFailed[0], "record-no-0");

    do_check_eq(called, 2);
    do_check_eq(counts.failed, 1);
    do_check_eq(counts.applied, 3);
    do_check_eq(counts.newFailed, 0);
    do_check_eq(counts.succeeded, 2);

    Svc.Obs.remove("weave:engine:sync:applied", onApplied);
  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_previousFailed() {
  _("Ensure that failed records are retried.");
  Service.identity.username = "foo";
  Svc.Prefs.set("client.type", "mobile");

  const APPLY_BATCH_SIZE = 4;
  const NUMBER_OF_RECORDS = 14;

  // Engine that fails the first 2 records.
  let engine = makeRotaryEngine();
  engine.mobileGUIDFetchBatchSize = engine.applyIncomingBatchSize = APPLY_BATCH_SIZE;
  engine._store._applyIncomingBatch = engine._store.applyIncomingBatch;
  engine._store.applyIncomingBatch = function(records) {
    engine._store._applyIncomingBatch(records.slice(2));
    return [records[0].id, records[1].id];
  };

  // Create a batch of server side records.
  let collection = new ServerCollection();
  for (var i = 0; i < NUMBER_OF_RECORDS; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + i});
    collection.insert(id, payload);
  }

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};
  try {
    // Confirm initial environment.
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.toFetch.length, 0);
    do_check_eq(engine.previousFailed.length, 0);
    do_check_empty(engine._store.items);

    // Initial failed items in previousFailed to be reset.
    let previousFailed = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.previousFailed = previousFailed;
    do_check_eq(engine.previousFailed, previousFailed);

    // Do sync.
    engine._syncStartup();
    engine._processIncoming();

    // Expected result: 4 sync batches with 2 failures each => 8 failures
    do_check_attribute_count(engine._store.items, 6);
    do_check_eq(engine.previousFailed.length, 8);
    do_check_eq(engine.previousFailed[0], "record-no-0");
    do_check_eq(engine.previousFailed[1], "record-no-1");
    do_check_eq(engine.previousFailed[2], "record-no-4");
    do_check_eq(engine.previousFailed[3], "record-no-5");
    do_check_eq(engine.previousFailed[4], "record-no-8");
    do_check_eq(engine.previousFailed[5], "record-no-9");
    do_check_eq(engine.previousFailed[6], "record-no-12");
    do_check_eq(engine.previousFailed[7], "record-no-13");

    // Sync again with the same failed items (records 0, 1, 8, 9).
    engine._processIncoming();

    // A second sync with the same failed items should not add the same items again.
    // Items that did not fail a second time should no longer be in previousFailed.
    do_check_attribute_count(engine._store.items, 10);
    do_check_eq(engine.previousFailed.length, 4);
    do_check_eq(engine.previousFailed[0], "record-no-0");
    do_check_eq(engine.previousFailed[1], "record-no-1");
    do_check_eq(engine.previousFailed[2], "record-no-8");
    do_check_eq(engine.previousFailed[3], "record-no-9");

    // Refetched items that didn't fail the second time are in engine._store.items.
    do_check_eq(engine._store.items['record-no-4'], "Record No. 4");
    do_check_eq(engine._store.items['record-no-5'], "Record No. 5");
    do_check_eq(engine._store.items['record-no-12'], "Record No. 12");
    do_check_eq(engine._store.items['record-no-13'], "Record No. 13");
  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_failed_records() {
  _("Ensure that failed records from _reconcile and applyIncomingBatch are refetched.");
  Service.identity.username = "foo";

  // Let's create three and a bit batches worth of server side records.
  let collection = new ServerCollection();
  const NUMBER_OF_RECORDS = MOBILE_BATCH_SIZE * 3 + 5;
  for (let i = 0; i < NUMBER_OF_RECORDS; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + id});
    let wbo = new ServerWBO(id, payload);
    wbo.modified = Date.now() / 1000 + 60 * (i - MOBILE_BATCH_SIZE * 3);
    collection.insertWBO(wbo);
  }

  // Engine that batches but likes to throw on a couple of records,
  // two in each batch: the even ones fail in reconcile, the odd ones
  // in applyIncoming.
  const BOGUS_RECORDS = ["record-no-" + 42,
                         "record-no-" + 23,
                         "record-no-" + (42 + MOBILE_BATCH_SIZE),
                         "record-no-" + (23 + MOBILE_BATCH_SIZE),
                         "record-no-" + (42 + MOBILE_BATCH_SIZE * 2),
                         "record-no-" + (23 + MOBILE_BATCH_SIZE * 2),
                         "record-no-" + (2 + MOBILE_BATCH_SIZE * 3),
                         "record-no-" + (1 + MOBILE_BATCH_SIZE * 3)];
  let engine = makeRotaryEngine();
  engine.applyIncomingBatchSize = MOBILE_BATCH_SIZE;

  engine.__reconcile = engine._reconcile;
  engine._reconcile = function _reconcile(record) {
    if (BOGUS_RECORDS.indexOf(record.id) % 2 == 0) {
      throw "I don't like this record! Baaaaaah!";
    }
    return this.__reconcile.apply(this, arguments);
  };
  engine._store._applyIncoming = engine._store.applyIncoming;
  engine._store.applyIncoming = function(record) {
    if (BOGUS_RECORDS.indexOf(record.id) % 2 == 1) {
      throw "I don't like this record! Baaaaaah!";
    }
    return this._applyIncoming.apply(this, arguments);
  };

  // Keep track of requests made of a collection.
  let count = 0;
  let uris  = [];
  function recording_handler(collection) {
    let h = collection.handler();
    return function(req, res) {
      ++count;
      uris.push(req.path + "?" + req.queryString);
      return h(req, res);
    };
  }
  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": recording_handler(collection)
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    // Confirm initial environment
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.toFetch.length, 0);
    do_check_eq(engine.previousFailed.length, 0);
    do_check_empty(engine._store.items);

    let observerSubject;
    let observerData;
    Svc.Obs.add("weave:engine:sync:applied", function onApplied(subject, data) {
      Svc.Obs.remove("weave:engine:sync:applied", onApplied);
      observerSubject = subject;
      observerData = data;
    });

    engine._syncStartup();
    engine._processIncoming();

    // Ensure that all records but the bogus 4 have been applied.
    do_check_attribute_count(engine._store.items,
                             NUMBER_OF_RECORDS - BOGUS_RECORDS.length);

    // Ensure that the bogus records will be fetched again on the next sync.
    do_check_eq(engine.previousFailed.length, BOGUS_RECORDS.length);
    engine.previousFailed.sort();
    BOGUS_RECORDS.sort();
    for (let i = 0; i < engine.previousFailed.length; i++) {
      do_check_eq(engine.previousFailed[i], BOGUS_RECORDS[i]);
    }

    // Ensure the observer was notified
    do_check_eq(observerData, engine.name);
    do_check_eq(observerSubject.failed, BOGUS_RECORDS.length);
    do_check_eq(observerSubject.newFailed, BOGUS_RECORDS.length);

    // Testing batching of failed item fetches.
    // Try to sync again. Ensure that we split the request into chunks to avoid
    // URI length limitations.
    function batchDownload(batchSize) {
      count = 0;
      uris  = [];
      engine.guidFetchBatchSize = batchSize;
      engine._processIncoming();
      _("Tried again. Requests: " + count + "; URIs: " + JSON.stringify(uris));
      return count;
    }

    // There are 8 bad records, so this needs 3 fetches.
    _("Test batching with ID batch size 3, normal mobile batch size.");
    do_check_eq(batchDownload(3), 3);

    // Now see with a more realistic limit.
    _("Test batching with sufficient ID batch size.");
    do_check_eq(batchDownload(BOGUS_RECORDS.length), 1);

    // If we're on mobile, that limit is used by default.
    _("Test batching with tiny mobile batch size.");
    Svc.Prefs.set("client.type", "mobile");
    engine.mobileGUIDFetchBatchSize = 2;
    do_check_eq(batchDownload(BOGUS_RECORDS.length), 4);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_processIncoming_decrypt_failed() {
  _("Ensure that records failing to decrypt are either replaced or refetched.");

  Service.identity.username = "foo";

  // Some good and some bogus records. One doesn't contain valid JSON,
  // the other will throw during decrypt.
  let collection = new ServerCollection();
  collection._wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));
  collection._wbos.nojson = new ServerWBO("nojson", "This is invalid JSON");
  collection._wbos.nojson2 = new ServerWBO("nojson2", "This is invalid JSON");
  collection._wbos.scotsman = new ServerWBO(
      'scotsman', encryptPayload({id: 'scotsman',
                                  denomination: "Flying Scotsman"}));
  collection._wbos.nodecrypt = new ServerWBO("nodecrypt", "Decrypt this!");
  collection._wbos.nodecrypt2 = new ServerWBO("nodecrypt2", "Decrypt this!");

  // Patch the fake crypto service to throw on the record above.
  Svc.Crypto._decrypt = Svc.Crypto.decrypt;
  Svc.Crypto.decrypt = function(ciphertext) {
    if (ciphertext == "Decrypt this!") {
      throw "Derp! Cipher finalized failed. Im ur crypto destroyin ur recordz.";
    }
    return this._decrypt.apply(this, arguments);
  };

  // Some broken records also exist locally.
  let engine = makeRotaryEngine();
  engine.enabled = true;
  engine._store.items = {nojson: "Valid JSON",
                         nodecrypt: "Valid ciphertext"};

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};
  try {

    // Confirm initial state
    do_check_eq(engine.toFetch.length, 0);
    do_check_eq(engine.previousFailed.length, 0);

    let observerSubject;
    let observerData;
    Svc.Obs.add("weave:engine:sync:applied", function onApplied(subject, data) {
      Svc.Obs.remove("weave:engine:sync:applied", onApplied);
      observerSubject = subject;
      observerData = data;
    });

    engine.lastSync = collection.wbo("nojson").modified - 1;
    let ping = await sync_engine_and_validate_telem(engine, true);
    do_check_eq(ping.engines[0].incoming.applied, 2);
    do_check_eq(ping.engines[0].incoming.failed, 4);
    do_check_eq(ping.engines[0].incoming.newFailed, 4);

    do_check_eq(engine.previousFailed.length, 4);
    do_check_eq(engine.previousFailed[0], "nojson");
    do_check_eq(engine.previousFailed[1], "nojson2");
    do_check_eq(engine.previousFailed[2], "nodecrypt");
    do_check_eq(engine.previousFailed[3], "nodecrypt2");

    // Ensure the observer was notified
    do_check_eq(observerData, engine.name);
    do_check_eq(observerSubject.applied, 2);
    do_check_eq(observerSubject.failed, 4);

  } finally {
    await promiseClean(engine, server);
  }
});


add_task(async function test_uploadOutgoing_toEmptyServer() {
  _("SyncEngine._uploadOutgoing uploads new records to server");

  Service.identity.username = "foo";
  let collection = new ServerCollection();
  collection._wbos.flying = new ServerWBO('flying');
  collection._wbos.scotsman = new ServerWBO('scotsman');

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler(),
      "/1.1/foo/storage/rotary/flying": collection.wbo("flying").handler(),
      "/1.1/foo/storage/rotary/scotsman": collection.wbo("scotsman").handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  generateNewKeys(Service.collectionKeys);

  let engine = makeRotaryEngine();
  engine.lastSync = 123; // needs to be non-zero so that tracker is queried
  engine._store.items = {flying: "LNER Class A3 4472",
                         scotsman: "Flying Scotsman"};
  // Mark one of these records as changed
  engine._tracker.addChangedID('scotsman', 0);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    // Confirm initial environment
    do_check_eq(engine.lastSyncLocal, 0);
    do_check_eq(collection.payload("flying"), undefined);
    do_check_eq(collection.payload("scotsman"), undefined);

    engine._syncStartup();
    engine._uploadOutgoing();

    // Local timestamp has been set.
    do_check_true(engine.lastSyncLocal > 0);

    // Ensure the marked record ('scotsman') has been uploaded and is
    // no longer marked.
    do_check_eq(collection.payload("flying"), undefined);
    do_check_true(!!collection.payload("scotsman"));
    do_check_eq(JSON.parse(collection.wbo("scotsman").data.ciphertext).id,
                "scotsman");
    do_check_eq(engine._tracker.changedIDs["scotsman"], undefined);

    // The 'flying' record wasn't marked so it wasn't uploaded
    do_check_eq(collection.payload("flying"), undefined);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_uploadOutgoing_huge() {
  Service.identity.username = "foo";
  let collection = new ServerCollection();
  collection._wbos.flying = new ServerWBO('flying');
  collection._wbos.scotsman = new ServerWBO('scotsman');

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler(),
      "/1.1/foo/storage/rotary/flying": collection.wbo("flying").handler(),
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  generateNewKeys(Service.collectionKeys);

  let engine = makeRotaryEngine();
  engine.allowSkippedRecord = true;
  engine.lastSync = 1;
  engine._store.items = { flying: "a".repeat(1024 * 1024) };

  engine._tracker.addChangedID("flying", 1000);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    // Confirm initial environment
    do_check_eq(engine.lastSyncLocal, 0);
    do_check_eq(collection.payload("flying"), undefined);

    engine._syncStartup();
    engine._uploadOutgoing();
    engine.trackRemainingChanges();

    // Check we didn't upload to the server
    do_check_eq(collection.payload("flying"), undefined);
    // And that we won't try to upload it again next time.
    do_check_eq(engine._tracker.changedIDs["flying"], undefined);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_uploadOutgoing_failed() {
  _("SyncEngine._uploadOutgoing doesn't clear the tracker of objects that failed to upload.");

  Service.identity.username = "foo";
  let collection = new ServerCollection();
  // We only define the "flying" WBO on the server, not the "scotsman"
  // and "peppercorn" ones.
  collection._wbos.flying = new ServerWBO('flying');

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let engine = makeRotaryEngine();
  engine.lastSync = 123; // needs to be non-zero so that tracker is queried
  engine._store.items = {flying: "LNER Class A3 4472",
                         scotsman: "Flying Scotsman",
                         peppercorn: "Peppercorn Class"};
  // Mark these records as changed
  const FLYING_CHANGED = 12345;
  const SCOTSMAN_CHANGED = 23456;
  const PEPPERCORN_CHANGED = 34567;
  engine._tracker.addChangedID('flying', FLYING_CHANGED);
  engine._tracker.addChangedID('scotsman', SCOTSMAN_CHANGED);
  engine._tracker.addChangedID('peppercorn', PEPPERCORN_CHANGED);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    // Confirm initial environment
    do_check_eq(engine.lastSyncLocal, 0);
    do_check_eq(collection.payload("flying"), undefined);
    do_check_eq(engine._tracker.changedIDs['flying'], FLYING_CHANGED);
    do_check_eq(engine._tracker.changedIDs['scotsman'], SCOTSMAN_CHANGED);
    do_check_eq(engine._tracker.changedIDs['peppercorn'], PEPPERCORN_CHANGED);

    engine.enabled = true;
    await sync_engine_and_validate_telem(engine, true);

    // Local timestamp has been set.
    do_check_true(engine.lastSyncLocal > 0);

    // Ensure the 'flying' record has been uploaded and is no longer marked.
    do_check_true(!!collection.payload("flying"));
    do_check_eq(engine._tracker.changedIDs['flying'], undefined);

    // The 'scotsman' and 'peppercorn' records couldn't be uploaded so
    // they weren't cleared from the tracker.
    do_check_eq(engine._tracker.changedIDs['scotsman'], SCOTSMAN_CHANGED);
    do_check_eq(engine._tracker.changedIDs['peppercorn'], PEPPERCORN_CHANGED);

  } finally {
    await promiseClean(engine, server);
  }
});

/* A couple of "functional" tests to ensure we split records into appropriate
   POST requests. More comprehensive unit-tests for this "batching" are in
   test_postqueue.js.
*/
add_task(async function test_uploadOutgoing_MAX_UPLOAD_RECORDS() {
  _("SyncEngine._uploadOutgoing uploads in batches of MAX_UPLOAD_RECORDS");

  Service.identity.username = "foo";
  let collection = new ServerCollection();

  // Let's count how many times the client posts to the server
  var noOfUploads = 0;
  collection.post = (function(orig) {
    return function(data, request) {
      // This test doesn't arrange for batch semantics - so we expect the
      // first request to come in with batch=true and the others to have no
      // batch related headers at all (as the first response did not provide
      // a batch ID)
      if (noOfUploads == 0) {
        do_check_eq(request.queryString, "batch=true");
      } else {
        do_check_eq(request.queryString, "");
      }
      noOfUploads++;
      return orig.call(this, data, request);
    };
  }(collection.post));

  // Create a bunch of records (and server side handlers)
  let engine = makeRotaryEngine();
  for (var i = 0; i < 234; i++) {
    let id = 'record-no-' + i;
    engine._store.items[id] = "Record No. " + i;
    engine._tracker.addChangedID(id, 0);
    collection.insert(id);
  }

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  try {

    // Confirm initial environment.
    do_check_eq(noOfUploads, 0);

    engine._syncStartup();
    engine._uploadOutgoing();

    // Ensure all records have been uploaded.
    for (i = 0; i < 234; i++) {
      do_check_true(!!collection.payload('record-no-' + i));
    }

    // Ensure that the uploads were performed in batches of MAX_UPLOAD_RECORDS.
    do_check_eq(noOfUploads, Math.ceil(234 / MAX_UPLOAD_RECORDS));

  } finally {
    await cleanAndGo(engine, server);
  }
});

add_task(async function test_uploadOutgoing_largeRecords() {
  _("SyncEngine._uploadOutgoing throws on records larger than MAX_UPLOAD_BYTES");

  Service.identity.username = "foo";
  let collection = new ServerCollection();

  let engine = makeRotaryEngine();
  engine.allowSkippedRecord = false;
  engine._store.items["large-item"] = "Y".repeat(MAX_UPLOAD_BYTES * 2);
  engine._tracker.addChangedID("large-item", 0);
  collection.insert("large-item");


  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  try {
    engine._syncStartup();
    let error = null;
    try {
      engine._uploadOutgoing();
    } catch (e) {
      error = e;
    }
    ok(!!error);
  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_syncFinish_noDelete() {
  _("SyncEngine._syncFinish resets tracker's score");

  let server = httpd_setup({});

  let syncTesting = await SyncTestingInfrastructure(server);
  let engine = makeRotaryEngine();
  engine._delete = {}; // Nothing to delete
  engine._tracker.score = 100;

  // _syncFinish() will reset the engine's score.
  engine._syncFinish();
  do_check_eq(engine.score, 0);
  server.stop(run_next_test);
});


add_task(async function test_syncFinish_deleteByIds() {
  _("SyncEngine._syncFinish deletes server records slated for deletion (list of record IDs).");

  Service.identity.username = "foo";
  let collection = new ServerCollection();
  collection._wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));
  collection._wbos.scotsman = new ServerWBO(
      'scotsman', encryptPayload({id: 'scotsman',
                                  denomination: "Flying Scotsman"}));
  collection._wbos.rekolok = new ServerWBO(
      'rekolok', encryptPayload({id: 'rekolok',
                                denomination: "Rekonstruktionslokomotive"}));

  let server = httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });
  let syncTesting = await SyncTestingInfrastructure(server);

  let engine = makeRotaryEngine();
  try {
    engine._delete = {ids: ['flying', 'rekolok']};
    engine._syncFinish();

    // The 'flying' and 'rekolok' records were deleted while the
    // 'scotsman' one wasn't.
    do_check_eq(collection.payload("flying"), undefined);
    do_check_true(!!collection.payload("scotsman"));
    do_check_eq(collection.payload("rekolok"), undefined);

    // The deletion todo list has been reset.
    do_check_eq(engine._delete.ids, undefined);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_syncFinish_deleteLotsInBatches() {
  _("SyncEngine._syncFinish deletes server records in batches of 100 (list of record IDs).");

  Service.identity.username = "foo";
  let collection = new ServerCollection();

  // Let's count how many times the client does a DELETE request to the server
  var noOfUploads = 0;
  collection.delete = (function(orig) {
    return function() {
      noOfUploads++;
      return orig.apply(this, arguments);
    };
  }(collection.delete));

  // Create a bunch of records on the server
  let now = Date.now();
  for (var i = 0; i < 234; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + i});
    let wbo = new ServerWBO(id, payload);
    wbo.modified = now / 1000 - 60 * (i + 110);
    collection.insertWBO(wbo);
  }

  let server = httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let engine = makeRotaryEngine();
  try {

    // Confirm initial environment
    do_check_eq(noOfUploads, 0);

    // Declare what we want to have deleted: all records no. 100 and
    // up and all records that are less than 200 mins old (which are
    // records 0 thru 90).
    engine._delete = {ids: [],
                      newer: now / 1000 - 60 * 200.5};
    for (i = 100; i < 234; i++) {
      engine._delete.ids.push('record-no-' + i);
    }

    engine._syncFinish();

    // Ensure that the appropriate server data has been wiped while
    // preserving records 90 thru 200.
    for (i = 0; i < 234; i++) {
      let id = 'record-no-' + i;
      if (i <= 90 || i >= 100) {
        do_check_eq(collection.payload(id), undefined);
      } else {
        do_check_true(!!collection.payload(id));
      }
    }

    // The deletion was done in batches
    do_check_eq(noOfUploads, 2 + 1);

    // The deletion todo list has been reset.
    do_check_eq(engine._delete.ids, undefined);

  } finally {
    await cleanAndGo(engine, server);
  }
});


add_task(async function test_sync_partialUpload() {
  _("SyncEngine.sync() keeps changedIDs that couldn't be uploaded.");

  Service.identity.username = "foo";

  let collection = new ServerCollection();
  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });
  let syncTesting = await SyncTestingInfrastructure(server);
  generateNewKeys(Service.collectionKeys);

  let engine = makeRotaryEngine();
  engine.lastSync = 123; // needs to be non-zero so that tracker is queried
  engine.lastSyncLocal = 456;

  // Let the third upload fail completely
  var noOfUploads = 0;
  collection.post = (function(orig) {
    return function() {
      if (noOfUploads == 2)
        throw "FAIL!";
      noOfUploads++;
      return orig.apply(this, arguments);
    };
  }(collection.post));

  // Create a bunch of records (and server side handlers)
  for (let i = 0; i < 234; i++) {
    let id = 'record-no-' + i;
    engine._store.items[id] = "Record No. " + i;
    engine._tracker.addChangedID(id, i);
    // Let two items in the first upload batch fail.
    if ((i != 23) && (i != 42)) {
      collection.insert(id);
    }
  }

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    engine.enabled = true;
    let error;
    try {
      await sync_engine_and_validate_telem(engine, true);
    } catch (ex) {
      error = ex;
    }

    ok(!!error);

    // The timestamp has been updated.
    do_check_true(engine.lastSyncLocal > 456);

    for (let i = 0; i < 234; i++) {
      let id = 'record-no-' + i;
      // Ensure failed records are back in the tracker:
      // * records no. 23 and 42 were rejected by the server,
      // * records no. 200 and higher couldn't be uploaded because we failed
      //   hard on the 3rd upload.
      if ((i == 23) || (i == 42) || (i >= 200))
        do_check_eq(engine._tracker.changedIDs[id], i);
      else
        do_check_false(id in engine._tracker.changedIDs);
    }

  } finally {
    await promiseClean(engine, server);
  }
});

add_task(async function test_canDecrypt_noCryptoKeys() {
  _("SyncEngine.canDecrypt returns false if the engine fails to decrypt items on the server, e.g. due to a missing crypto key collection.");
  Service.identity.username = "foo";

  // Wipe collection keys so we can test the desired scenario.
  Service.collectionKeys.clear();

  let collection = new ServerCollection();
  collection._wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  let engine = makeRotaryEngine();
  try {

    do_check_false(engine.canDecrypt());

  } finally {
    await cleanAndGo(engine, server);
  }
});

add_task(async function test_canDecrypt_true() {
  _("SyncEngine.canDecrypt returns true if the engine can decrypt the items on the server.");
  Service.identity.username = "foo";

  generateNewKeys(Service.collectionKeys);

  let collection = new ServerCollection();
  collection._wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);
  let engine = makeRotaryEngine();
  try {

    do_check_true(engine.canDecrypt());

  } finally {
    await cleanAndGo(engine, server);
  }

});

add_task(async function test_syncapplied_observer() {
  Service.identity.username = "foo";

  const NUMBER_OF_RECORDS = 10;

  let engine = makeRotaryEngine();

  // Create a batch of server side records.
  let collection = new ServerCollection();
  for (var i = 0; i < NUMBER_OF_RECORDS; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id, denomination: "Record No. " + id});
    collection.insert(id, payload);
  }

  let server = httpd_setup({
    "/1.1/foo/storage/rotary": collection.handler()
  });

  let syncTesting = await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                         syncID: engine.syncID}};

  let numApplyCalls = 0;
  let engine_name;
  let count;
  function onApplied(subject, data) {
    numApplyCalls++;
    engine_name = data;
    count = subject;
  }

  Svc.Obs.add("weave:engine:sync:applied", onApplied);

  try {
    Service.scheduler.hasIncomingItems = false;

    // Do sync.
    engine._syncStartup();
    engine._processIncoming();

    do_check_attribute_count(engine._store.items, 10);

    do_check_eq(numApplyCalls, 1);
    do_check_eq(engine_name, "rotary");
    do_check_eq(count.applied, 10);

    do_check_true(Service.scheduler.hasIncomingItems);
  } finally {
    await cleanAndGo(engine, server);
    Service.scheduler.hasIncomingItems = false;
    Svc.Obs.remove("weave:engine:sync:applied", onApplied);
  }
});
