Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");

/*
 * A fake engine implementation.
 * 
 * Complete with record, store, and tracker implementations.
 */

function SteamRecord(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
SteamRecord.prototype = {
  __proto__: CryptoWrapper.prototype
};
Utils.deferGetSet(SteamRecord, "cleartext", ["denomination"]);

function SteamStore() {
  Store.call(this, "Steam");
  this.items = {};
}
SteamStore.prototype = {
  __proto__: Store.prototype,

  create: function Store_create(record) {
    this.items[record.id] = record.denomination;
  },

  remove: function Store_remove(record) {
    delete this.items[record.id];
  },

  update: function Store_update(record) {
    this.items[record.id] = record.denomination;
  },

  itemExists: function Store_itemExists(id) {
    return (id in this.items);
  },

  createRecord: function(id, collection) {
    var record = new SteamRecord(collection, id);
    record.denomination = this.items[id] || "Data for new record: " + id;
    return record;
  },

  changeItemID: function(oldID, newID) {
    this.items[newID] = this.items[oldID];
    delete this.items[oldID];
  },

  getAllIDs: function() {
    let ids = {};
    for (var id in this.items) {
      ids[id] = true;
    }
    return ids;
  },

  wipe: function() {
    this.items = {};
  }
};

function SteamTracker() {
  Tracker.call(this, "Steam");
}
SteamTracker.prototype = {
  __proto__: Tracker.prototype
};


function SteamEngine() {
  SyncEngine.call(this, "Steam");
}
SteamEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: SteamStore,
  _trackerObj: SteamTracker,
  _recordObj: SteamRecord,

  _findDupe: function(item) {
    for (let [id, value] in Iterator(this._store.items)) {
      if (item.denomination == value) {
        return id;
      }
    }
  }
};


function makeSteamEngine() {
  return new SteamEngine();
}

var syncTesting = new SyncTestingInfrastructure(makeSteamEngine);

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

function test_syncStartup_emptyOrOutdatedGlobalsResetsSync() {
  _("SyncEngine._syncStartup resets sync and wipes server data if there's no or an outdated global record");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");

  // Some server side data that's going to be wiped
  let collection = new ServerCollection();
  collection.wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));
  collection.wbos.scotsman = new ServerWBO(
      'scotsman', encryptPayload({id: 'scotsman',
                                  denomination: "Flying Scotsman"}));

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
  engine._store.items = {rekolok: "Rekonstruktionslokomotive"};
  try {

    // Confirm initial environment
    do_check_eq(engine._tracker.changedIDs["rekolok"], undefined);
    let metaGlobal = Records.get(engine.metaURL);
    do_check_eq(metaGlobal.payload.engines, undefined);
    do_check_true(!!collection.wbos.flying.payload);
    do_check_true(!!collection.wbos.scotsman.payload);

    engine.lastSync = Date.now() / 1000;
    engine.lastSyncLocal = Date.now();
    
    // Trying to prompt a wipe -- we no longer track CryptoMeta per engine,
    // so it has nothing to check.
    engine._syncStartup();

    // The meta/global WBO has been filled with data about the engine
    let engineData = metaGlobal.payload.engines["steam"];
    do_check_eq(engineData.version, engine.version);
    do_check_eq(engineData.syncID, engine.syncID);

    // Sync was reset and server data was wiped
    do_check_eq(engine.lastSync, 0);
    do_check_eq(collection.wbos.flying.payload, undefined);
    do_check_eq(collection.wbos.scotsman.payload, undefined);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}

function test_syncStartup_serverHasNewerVersion() {
  _("SyncEngine._syncStartup ");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  let global = new ServerWBO('global', {engines: {steam: {version: 23456}}});
  let server = httpd_setup({
      "/1.0/foo/storage/meta/global": global.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
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
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_syncStartup_syncIDMismatchResetsClient() {
  _("SyncEngine._syncStartup resets sync if syncIDs don't match");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  let server = sync_httpd_setup({});
  do_test_pending();

  // global record with a different syncID than our engine has
  let engine = makeSteamEngine();
  let global = new ServerWBO('global',
                             {engines: {steam: {version: engine.version,
                                                syncID: 'foobar'}}});
  server.registerPathHandler("/1.0/foo/storage/meta/global", global.handler());

  try {

    // Confirm initial environment
    do_check_eq(engine.syncID, 'fake-guid-0');
    do_check_eq(engine._tracker.changedIDs["rekolok"], undefined);

    engine.lastSync = Date.now() / 1000;
    engine.lastSyncLocal = Date.now();
    engine._syncStartup();

    // The engine has assumed the server's syncID 
    do_check_eq(engine.syncID, 'foobar');

    // Sync was reset
    do_check_eq(engine.lastSync, 0);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_processIncoming_emptyServer() {
  _("SyncEngine._processIncoming working with an empty server backend");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  let collection = new ServerCollection();

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
  try {

    // Merely ensure that this code path is run without any errors
    engine._processIncoming();
    do_check_eq(engine.lastSync, 0);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_processIncoming_createFromServer() {
  _("SyncEngine._processIncoming creates new records from server data");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  
  CollectionKeys.generateNewKeys();

  // Some server records that will be downloaded
  let collection = new ServerCollection();
  collection.wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));
  collection.wbos.scotsman = new ServerWBO(
      'scotsman', encryptPayload({id: 'scotsman',
                                  denomination: "Flying Scotsman"}));

  // Two pathological cases involving relative URIs gone wrong.
  collection.wbos['../pathological'] = new ServerWBO(
      '../pathological', encryptPayload({id: '../pathological',
                                         denomination: "Pathological Case"}));

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler(),
      "/1.0/foo/storage/steam/flying": collection.wbos.flying.handler(),
      "/1.0/foo/storage/steam/scotsman": collection.wbos.scotsman.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
  let meta_global = Records.set(engine.metaURL, new WBORecord(engine.metaURL));
  meta_global.payload.engines = {steam: {version: engine.version,
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
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_processIncoming_reconcile() {
  _("SyncEngine._processIncoming updates local records");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  let collection = new ServerCollection();

  // This server record is newer than the corresponding client one,
  // so it'll update its data.
  collection.wbos.newrecord = new ServerWBO(
      'newrecord', encryptPayload({id: 'newrecord',
                                   denomination: "New stuff..."}));

  // This server record is newer than the corresponding client one,
  // so it'll update its data.
  collection.wbos.newerserver = new ServerWBO(
      'newerserver', encryptPayload({id: 'newerserver',
                                     denomination: "New data!"}));

  // This server record is 2 mins older than the client counterpart
  // but identical to it, so we're expecting the client record's
  // changedID to be reset.
  collection.wbos.olderidentical = new ServerWBO(
      'olderidentical', encryptPayload({id: 'olderidentical',
                                        denomination: "Older but identical"}));
  collection.wbos.olderidentical.modified -= 120;

  // This item simply has different data than the corresponding client
  // record (which is unmodified), so it will update the client as well
  collection.wbos.updateclient = new ServerWBO(
      'updateclient', encryptPayload({id: 'updateclient',
                                      denomination: "Get this!"}));

  // This is a dupe of 'original' but with a longer GUID, so we're
  // expecting it to be marked for deletion from the server
  collection.wbos.duplication = new ServerWBO(
      'duplication', encryptPayload({id: 'duplication',
                                     denomination: "Original Entry"}));

  // This is a dupe of 'long_original' but with a shorter GUID, so we're
  // expecting it to replace 'long_original'.
  collection.wbos.dupe = new ServerWBO(
      'dupe', encryptPayload({id: 'dupe',
                              denomination: "Long Original Entry"}));  

  // This record is marked as deleted, so we're expecting the client
  // record to be removed.
  collection.wbos.nukeme = new ServerWBO(
      'nukeme', encryptPayload({id: 'nukeme',
                                denomination: "Nuke me!",
                                deleted: true}));

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
  engine._store.items = {newerserver: "New data, but not as new as server!",
                         olderidentical: "Older but identical",
                         updateclient: "Got data?",
                         original: "Original Entry",
                         long_original: "Long Original Entry",
                         nukeme: "Nuke me!"};
  // Make this record 1 min old, thus older than the one on the server
  engine._tracker.addChangedID('newerserver', Date.now()/1000 - 60);
  // This record has been changed 2 mins later than the one on the server
  engine._tracker.addChangedID('olderidentical', Date.now()/1000);

  let meta_global = Records.set(engine.metaURL, new WBORecord(engine.metaURL));
  meta_global.payload.engines = {steam: {version: engine.version,
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

    // The dupe with the shorter ID is kept, the longer one is slated
    // for deletion.
    do_check_eq(engine._store.items.long_original, undefined);
    do_check_eq(engine._store.items.dupe, "Long Original Entry");
    do_check_neq(engine._delete.ids.indexOf('duplication'), -1);

    // The 'nukeme' record marked as deleted is removed.
    do_check_eq(engine._store.items.nukeme, undefined);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_processIncoming_mobile_batchSize() {
  _("SyncEngine._processIncoming doesn't fetch everything at once on mobile clients");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  Svc.Prefs.set("client.type", "mobile");
  let crypto_steam = new ServerWBO('steam');

  // A collection that logs each GET
  let collection = new ServerCollection();
  collection.get_log = [];
  collection._get = collection.get;
  collection.get = function (options) {
    this.get_log.push(options);
    return this._get(options);
  };

  // Let's create some 234 server side records. They're all at least
  // 10 minutes old.
  for (var i = 0; i < 234; i++) {
    let id = 'record-no-' + i;
    let payload = encryptPayload({id: id, denomination: "Record No. " + i});
    let wbo = new ServerWBO(id, payload);
    wbo.modified = Date.now()/1000 - 60*(i+10);
    collection.wbos[id] = wbo;
  }

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
  let meta_global = Records.set(engine.metaURL, new WBORecord(engine.metaURL));
  meta_global.payload.engines = {steam: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    _("On a mobile client, we get new records from the server in batches of 50.");
    engine._syncStartup();
    engine._processIncoming();
    do_check_eq([id for (id in engine._store.items)].length, 234);
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
      do_check_eq(collection.get_log[i+1].full, 1);
      do_check_eq(collection.get_log[i+1].limit, undefined);
      if (i < Math.floor(234 / MOBILE_BATCH_SIZE))
        do_check_eq(collection.get_log[i+1].ids.length, MOBILE_BATCH_SIZE);
      else
        do_check_eq(collection.get_log[i+1].ids.length, 234 % MOBILE_BATCH_SIZE);
    }

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}

function test_uploadOutgoing_toEmptyServer() {
  _("SyncEngine._uploadOutgoing uploads new records to server");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  let collection = new ServerCollection();
  collection.wbos.flying = new ServerWBO('flying');
  collection.wbos.scotsman = new ServerWBO('scotsman');

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler(),
      "/1.0/foo/storage/steam/flying": collection.wbos.flying.handler(),
      "/1.0/foo/storage/steam/scotsman": collection.wbos.scotsman.handler()
  });
  do_test_pending();
  CollectionKeys.generateNewKeys();

  let engine = makeSteamEngine();
  engine.lastSync = 123; // needs to be non-zero so that tracker is queried
  engine._store.items = {flying: "LNER Class A3 4472",
                         scotsman: "Flying Scotsman"};
  // Mark one of these records as changed 
  engine._tracker.addChangedID('scotsman', 0);

  let meta_global = Records.set(engine.metaURL, new WBORecord(engine.metaURL));
  meta_global.payload.engines = {steam: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    // Confirm initial environment
    do_check_eq(engine.lastSyncLocal, 0);
    do_check_eq(collection.wbos.flying.payload, undefined);
    do_check_eq(collection.wbos.scotsman.payload, undefined);

    engine._syncStartup();
    engine._uploadOutgoing();

    // Local timestamp has been set.
    do_check_true(engine.lastSyncLocal > 0);

    // Ensure the marked record ('scotsman') has been uploaded and is
    // no longer marked.
    do_check_eq(collection.wbos.flying.payload, undefined);
    do_check_true(!!collection.wbos.scotsman.payload);
    do_check_eq(JSON.parse(collection.wbos.scotsman.data.ciphertext).id,
                'scotsman');
    do_check_eq(engine._tracker.changedIDs['scotsman'], undefined);

    // The 'flying' record wasn't marked so it wasn't uploaded
    do_check_eq(collection.wbos.flying.payload, undefined);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_uploadOutgoing_failed() {
  _("SyncEngine._uploadOutgoing doesn't clear the tracker of objects that failed to upload.");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  let collection = new ServerCollection();
  // We only define the "flying" WBO on the server, not the "scotsman"
  // and "peppercorn" ones.
  collection.wbos.flying = new ServerWBO('flying');

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
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

  let meta_global = Records.set(engine.metaURL, new WBORecord(engine.metaURL));
  meta_global.payload.engines = {steam: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    // Confirm initial environment
    do_check_eq(engine.lastSyncLocal, 0);
    do_check_eq(collection.wbos.flying.payload, undefined);
    do_check_eq(engine._tracker.changedIDs['flying'], FLYING_CHANGED);
    do_check_eq(engine._tracker.changedIDs['scotsman'], SCOTSMAN_CHANGED);
    do_check_eq(engine._tracker.changedIDs['peppercorn'], PEPPERCORN_CHANGED);

    engine.enabled = true;
    engine.sync();

    // Local timestamp has been set.
    do_check_true(engine.lastSyncLocal > 0);

    // Ensure the 'flying' record has been uploaded and is no longer marked.
    do_check_true(!!collection.wbos.flying.payload);
    do_check_eq(engine._tracker.changedIDs['flying'], undefined);

    // The 'scotsman' and 'peppercorn' records couldn't be uploaded so
    // they weren't cleared from the tracker.
    do_check_eq(engine._tracker.changedIDs['scotsman'], SCOTSMAN_CHANGED);
    do_check_eq(engine._tracker.changedIDs['peppercorn'], PEPPERCORN_CHANGED);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_uploadOutgoing_MAX_UPLOAD_RECORDS() {
  _("SyncEngine._uploadOutgoing uploads in batches of MAX_UPLOAD_RECORDS");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  let collection = new ServerCollection();

  // Let's count how many times the client posts to the server
  var noOfUploads = 0;
  collection.post = (function(orig) {
    return function() {
      noOfUploads++;
      return orig.apply(this, arguments);
    };
  }(collection.post));

  // Create a bunch of records (and server side handlers)
  let engine = makeSteamEngine();
  for (var i = 0; i < 234; i++) {
    let id = 'record-no-' + i;
    engine._store.items[id] = "Record No. " + i;
    engine._tracker.addChangedID(id, 0);
    collection.wbos[id] = new ServerWBO(id);
  }

  let meta_global = Records.set(engine.metaURL, new WBORecord(engine.metaURL));
  meta_global.payload.engines = {steam: {version: engine.version,
                                         syncID: engine.syncID}};

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  try {

    // Confirm initial environment
    do_check_eq(noOfUploads, 0);

    engine._syncStartup();
    engine._uploadOutgoing();

    // Ensure all records have been uploaded
    for (i = 0; i < 234; i++) {
      do_check_true(!!collection.wbos['record-no-'+i].payload);
    }

    // Ensure that the uploads were performed in batches of MAX_UPLOAD_RECORDS
    do_check_eq(noOfUploads, Math.ceil(234/MAX_UPLOAD_RECORDS));

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_syncFinish_noDelete() {
  _("SyncEngine._syncFinish resets tracker's score");
  let engine = makeSteamEngine();
  engine._delete = {}; // Nothing to delete
  engine._tracker.score = 100;

  // _syncFinish() will reset the engine's score.
  engine._syncFinish();
  do_check_eq(engine.score, 0);
}


function test_syncFinish_deleteByIds() {
  _("SyncEngine._syncFinish deletes server records slated for deletion (list of record IDs).");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
  let collection = new ServerCollection();
  collection.wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));
  collection.wbos.scotsman = new ServerWBO(
      'scotsman', encryptPayload({id: 'scotsman',
                                  denomination: "Flying Scotsman"}));
  collection.wbos.rekolok = new ServerWBO(
      'rekolok', encryptPayload({id: 'rekolok',
                                denomination: "Rekonstruktionslokomotive"}));

  let server = httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
  try {
    engine._delete = {ids: ['flying', 'rekolok']};
    engine._syncFinish();

    // The 'flying' and 'rekolok' records were deleted while the
    // 'scotsman' one wasn't.
    do_check_eq(collection.wbos.flying.payload, undefined);
    do_check_true(!!collection.wbos.scotsman.payload);
    do_check_eq(collection.wbos.rekolok.payload, undefined);

    // The deletion todo list has been reset.
    do_check_eq(engine._delete.ids, undefined);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_syncFinish_deleteLotsInBatches() {
  _("SyncEngine._syncFinish deletes server records in batches of 100 (list of record IDs).");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");
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
    let payload = encryptPayload({id: id, denomination: "Record No. " + i});
    let wbo = new ServerWBO(id, payload);
    wbo.modified = now / 1000 - 60 * (i + 110);
    collection.wbos[id] = wbo;
  }

  let server = httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
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
        do_check_eq(collection.wbos[id].payload, undefined);
      } else {
        do_check_true(!!collection.wbos[id].payload);
      }
    }

    // The deletion was done in batches
    do_check_eq(noOfUploads, 2 + 1);

    // The deletion todo list has been reset.
    do_check_eq(engine._delete.ids, undefined);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function test_sync_partialUpload() {
  _("SyncEngine.sync() keeps changedIDs that couldn't be uploaded.");

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");

  let crypto_steam = new ServerWBO('steam');
  let collection = new ServerCollection();
  let server = sync_httpd_setup({
      "/1.0/foo/storage/crypto/steam": crypto_steam.handler(),
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();
  CollectionKeys.generateNewKeys();

  let engine = makeSteamEngine();
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
    if ((i != 23) && (i != 42))
      collection.wbos[id] = new ServerWBO(id);
  }

  let meta_global = Records.set(engine.metaURL, new WBORecord(engine.metaURL));
  meta_global.payload.engines = {steam: {version: engine.version,
                                         syncID: engine.syncID}};

  try {

    engine.enabled = true;
    let error;
    try {
      engine.sync();
    } catch (ex) {
      error = ex;
    }
    do_check_true(!!error);

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
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}

function test_canDecrypt_noCryptoKeys() {
  _("SyncEngine.canDecrypt returns false if the engine fails to decrypt items on the server, e.g. due to a missing crypto key collection.");
  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");

  // Wipe CollectionKeys so we can test the desired scenario.
  CollectionKeys.clear();

  let collection = new ServerCollection();
  collection.wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
  try {

    do_check_false(engine.canDecrypt());

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}

function test_canDecrypt_true() {
  _("SyncEngine.canDecrypt returns true if the engine can decrypt the items on the server.");
  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");

  // Set up CollectionKeys, as service.js does.
  CollectionKeys.generateNewKeys();
  
  let collection = new ServerCollection();
  collection.wbos.flying = new ServerWBO(
      'flying', encryptPayload({id: 'flying',
                                denomination: "LNER Class A3 4472"}));

  let server = sync_httpd_setup({
      "/1.0/foo/storage/steam": collection.handler()
  });
  do_test_pending();

  let engine = makeSteamEngine();
  try {

    do_check_true(engine.canDecrypt());

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}


function run_test() {
  if (DISABLE_TESTS_BUG_604565)
    return;

  CollectionKeys.generateNewKeys();

  test_syncStartup_emptyOrOutdatedGlobalsResetsSync();
  test_syncStartup_serverHasNewerVersion();
  test_syncStartup_syncIDMismatchResetsClient();
  test_processIncoming_emptyServer();
  test_processIncoming_createFromServer();
  test_processIncoming_reconcile();
  test_processIncoming_mobile_batchSize();
  test_uploadOutgoing_toEmptyServer();
  test_uploadOutgoing_failed();
  test_uploadOutgoing_MAX_UPLOAD_RECORDS();
  test_syncFinish_noDelete();
  test_syncFinish_deleteByIds();
  test_syncFinish_deleteLotsInBatches();
  test_sync_partialUpload();
  test_canDecrypt_noCryptoKeys();
  test_canDecrypt_true();
}
