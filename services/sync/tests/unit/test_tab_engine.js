/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

async function getMocks() {
  let engine = new TabEngine(Service);
  await engine.initialize();
  let store = engine._store;
  store.getTabState = mockGetTabState;
  store.shouldSkipWindow = mockShouldSkipWindow;
  return [engine, store];
}

add_task(async function test_getOpenURLs() {
  _("Test getOpenURLs.");
  let [engine, store] = await getMocks();

  let superLongURL = "http://" + (new Array(MAX_UPLOAD_BYTES).join("w")) + ".com/";
  let urls = ["http://bar.com", "http://foo.com", "http://foobar.com", superLongURL];
  function fourURLs() {
    return urls.pop();
  }
  store.getWindowEnumerator = mockGetWindowEnumerator.bind(this, fourURLs, 1, 4);

  let matches;

  _("  test matching works (true)");
  let openurlsset = engine.getOpenURLs();
  matches = openurlsset.has("http://foo.com");
  ok(matches);

  _("  test matching works (false)");
  matches = openurlsset.has("http://barfoo.com");
  ok(!matches);

  _("  test matching works (too long)");
  matches = openurlsset.has(superLongURL);
  ok(!matches);
});

add_task(async function test_tab_engine_skips_incoming_local_record() {
  _("Ensure incoming records that match local client ID are never applied.");
  let [engine, store] = await getMocks();
  let localID = engine.service.clientsEngine.localID;
  let apply = store.applyIncoming;
  let applied = [];

  store.applyIncoming = async function(record) {
    notEqual(record.id, localID, "Only apply tab records from remote clients");
    applied.push(record);
    apply.call(store, record);
  }

  let collection = new ServerCollection();

  _("Creating remote tab record with local client ID");
  let localRecord = encryptPayload({id: localID, clientName: "local"});
  collection.insert(localID, localRecord);

  _("Creating remote tab record with a different client ID");
  let remoteID = "different";
  let remoteRecord = encryptPayload({id: remoteID, clientName: "not local"});
  collection.insert(remoteID, remoteRecord);

  _("Setting up Sync server");
  let server = sync_httpd_setup({
      "/1.1/foo/storage/tabs": collection.handler()
  });

  await SyncTestingInfrastructure(server);

  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {tabs: {version: engine.version,
                                        syncID: engine.syncID}};

  generateNewKeys(Service.collectionKeys);

  let promiseFinished = new Promise(resolve => {
    let syncFinish = engine._syncFinish;
    engine._syncFinish = async function() {
      equal(applied.length, 1, "Remote client record was applied");
      equal(applied[0].id, remoteID, "Remote client ID matches");

      await syncFinish.call(engine);
      resolve();
    }
  });

  _("Start sync");
  await engine._sync();
  await promiseFinished;
});

add_task(async function test_reconcile() {
  let [engine, ] = await getMocks();

  _("Setup engine for reconciling");
  await engine._syncStartup();

  _("Create an incoming remote record");
  let remoteRecord = {id: "remote id",
                      cleartext: "stuff and things!",
                      modified: 1000};

  ok((await engine._reconcile(remoteRecord)), "Apply a recently modified remote record");

  remoteRecord.modified = 0;
  ok((await engine._reconcile(remoteRecord)), "Apply a remote record modified long ago");

  // Remote tab records are never tracked locally, so the only
  // time they're skipped is when they're marked as deleted.
  remoteRecord.deleted = true;
  ok(!(await engine._reconcile(remoteRecord)), "Skip a deleted remote record");

  _("Create an incoming local record");
  // The locally tracked tab record always takes precedence over its
  // remote counterparts.
  let localRecord = {id: engine.service.clientsEngine.localID,
                     cleartext: "this should always be skipped",
                     modified: 2000};

  ok(!(await engine._reconcile(localRecord)), "Skip incoming local if recently modified");

  localRecord.modified = 0;
  ok(!(await engine._reconcile(localRecord)), "Skip incoming local if modified long ago");

  localRecord.deleted = true;
  ok(!(await engine._reconcile(localRecord)), "Skip incoming local if deleted");
});
