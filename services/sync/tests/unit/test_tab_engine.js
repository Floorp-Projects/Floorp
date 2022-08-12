/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabEngine } = ChromeUtils.import(
  "resource://services-sync/engines/tabs.js"
);
const { WBORecord } = ChromeUtils.import("resource://services-sync/record.js");
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

async function getMocks() {
  let engine = new TabEngine(Service);
  await engine.initialize();
  let store = engine._store;
  store.getTabState = mockGetTabState;
  store.shouldSkipWindow = mockShouldSkipWindow;
  return [engine, store];
}

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
  };

  let collection = new ServerCollection();

  _("Creating remote tab record with local client ID");
  let localRecord = encryptPayload({ id: localID, clientName: "local" });
  collection.insert(localID, localRecord);

  _("Creating remote tab record with a different client ID");
  let remoteID = "different";
  let remoteRecord = encryptPayload({ id: remoteID, clientName: "not local" });
  collection.insert(remoteID, remoteRecord);

  _("Setting up Sync server");
  let server = sync_httpd_setup({
    "/1.1/foo/storage/tabs": collection.handler(),
  });

  await SyncTestingInfrastructure(server);

  let syncID = await engine.resetLocalSyncID();
  let meta_global = Service.recordManager.set(
    engine.metaURL,
    new WBORecord(engine.metaURL)
  );
  meta_global.payload.engines = { tabs: { version: engine.version, syncID } };

  await generateNewKeys(Service.collectionKeys);

  let promiseFinished = new Promise(resolve => {
    let syncFinish = engine._syncFinish;
    engine._syncFinish = async function() {
      equal(applied.length, 1, "Remote client record was applied");
      equal(applied[0].id, remoteID, "Remote client ID matches");

      await syncFinish.call(engine);
      resolve();
    };
  });

  _("Start sync");
  await engine._sync();
  await promiseFinished;
});

add_task(async function test_reconcile() {
  let [engine] = await getMocks();

  _("Setup engine for reconciling");
  await engine._syncStartup();

  _("Create an incoming remote record");
  let remoteRecord = {
    id: "remote id",
    cleartext: "stuff and things!",
    modified: 1000,
  };

  ok(
    await engine._reconcile(remoteRecord),
    "Apply a recently modified remote record"
  );

  remoteRecord.modified = 0;
  ok(
    await engine._reconcile(remoteRecord),
    "Apply a remote record modified long ago"
  );

  // Remote tab records are never tracked locally, so the only
  // time they're skipped is when they're marked as deleted.
  remoteRecord.deleted = true;
  ok(!(await engine._reconcile(remoteRecord)), "Skip a deleted remote record");

  _("Create an incoming local record");
  // The locally tracked tab record always takes precedence over its
  // remote counterparts.
  let localRecord = {
    id: engine.service.clientsEngine.localID,
    cleartext: "this should always be skipped",
    modified: 2000,
  };

  ok(
    !(await engine._reconcile(localRecord)),
    "Skip incoming local if recently modified"
  );

  localRecord.modified = 0;
  ok(
    !(await engine._reconcile(localRecord)),
    "Skip incoming local if modified long ago"
  );

  localRecord.deleted = true;
  ok(!(await engine._reconcile(localRecord)), "Skip incoming local if deleted");
});

add_task(async function test_modified_after_fail() {
  let [engine, store] = await getMocks();
  store.getWindowEnumerator = () =>
    mockGetWindowEnumerator(["http://example.com"]);

  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  try {
    _("First sync; create collection and tabs record on server");
    await sync_engine_and_validate_telem(engine, false);

    let collection = server.user("foo").collection("tabs");
    deepEqual(
      collection.cleartext(engine.service.clientsEngine.localID).tabs,
      [
        {
          title: "title",
          urlHistory: ["http://example.com/"],
          icon: "",
          lastUsed: 2,
        },
      ],
      "Should upload mock local tabs on first sync"
    );
    ok(
      !engine._tracker.modified,
      "Tracker shouldn't be modified after first sync"
    );

    _("Second sync; flag tracker as modified and throw on upload");
    engine._tracker.modified = true;
    let oldPost = collection.post;
    collection.post = () => {
      throw new Error("Sync this!");
    };
    await Assert.rejects(
      sync_engine_and_validate_telem(engine, true),
      ex => ex.success === false
    );
    ok(
      engine._tracker.modified,
      "Tracker should remain modified after failed sync"
    );

    _("Third sync");
    collection.post = oldPost;
    await sync_engine_and_validate_telem(engine, false);
    ok(
      !engine._tracker.modified,
      "Tracker shouldn't be modified again after third sync"
    );
  } finally {
    await promiseStopServer(server);
  }
});
