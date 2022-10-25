/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://services-sync/engines/tabs.js");
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

const { TabProvider } = ChromeUtils.import(
  "resource://services-sync/engines/tabs.js"
);

let engine;

add_task(async function setup() {
  engine = Service.engineManager.get("tabs");
  await engine.initialize();

  // Since these are xpcshell tests, we'll need to mock ui features
  TabProvider.shouldSkipWindow = mockShouldSkipWindow;
  TabProvider.getWindowEnumerator = mockGetWindowEnumerator.bind(this, [
    "http://foo.com",
  ]);
});

async function prepareServer() {
  _("Setting up Sync server");
  Service.serverConfiguration = {
    max_post_records: 100,
  };

  let server = new SyncServer();
  server.start();
  await SyncTestingInfrastructure(server, "username");
  server.registerUser("username");

  let collection = server.createCollection("username", "tabs");
  await generateNewKeys(Service.collectionKeys);
  return { server, collection };
}

async function withPatchedValue(object, name, patchedVal, fn) {
  _(`patching ${name}=${patchedVal}`);
  let old = object[name];
  object[name] = patchedVal;
  try {
    await fn();
  } finally {
    object[name] = old;
  }
}

add_task(async function test_tab_quickwrite_works() {
  _("Ensure a simple quickWrite works.");
  let { server, collection } = await prepareServer();
  Assert.equal(collection.count(), 0, "starting with 0 tab records");
  await engine.quickWrite();
  Assert.equal(collection.count(), 1, "tab record was written");

  await promiseStopServer(server);
});

add_task(async function test_tab_bad_status() {
  _("Ensure quickWrite silently aborts when we aren't setup correctly.");
  let { server } = await prepareServer();
  // Store the original lock to reset it back after this test
  let lock = engine.lock;
  // Arrange for this test to fail if it tries to take the lock.
  engine.lock = function() {
    throw new Error("this test should abort syncing before locking");
  };
  let quickWrite = engine.quickWrite.bind(engine); // lol javascript.

  await withPatchedValue(engine, "enabled", false, quickWrite);
  await withPatchedValue(Service, "serverConfiguration", null, quickWrite);

  Services.prefs.clearUserPref("services.sync.username");
  quickWrite();
  Service.status.resetSync();
  engine.lock = lock;
  await promiseStopServer(server);
});

add_task(async function test_tab_quickwrite_lock() {
  _("Ensure we fail to quickWrite if the engine is locked.");
  let { server, collection } = await prepareServer();

  Assert.equal(collection.count(), 0, "starting with 0 tab records");
  engine.lock();
  await engine.quickWrite();
  Assert.equal(collection.count(), 0, "didn't sync due to being locked");
  engine.unlock();

  await promiseStopServer(server);
});

add_task(async function test_tab_lastSync() {
  _("Ensure we restore the lastSync timestamp after a quick-write.");
  let { server, collection } = await prepareServer();

  let origLastSync = engine.lastSync;
  await engine.quickWrite();
  Assert.equal(engine.lastSync, origLastSync);
  Assert.equal(collection.count(), 1, "successful sync");
  engine.unlock();

  await promiseStopServer(server);
});

add_task(async function test_tab_quickWrite_telemetry() {
  _("Ensure we record the telemetry we expect.");
  // hook into telemetry
  let telem = get_sync_test_telemetry();
  telem.payloads = [];
  let oldSubmit = telem.submit;
  let submitPromise = new Promise((resolve, reject) => {
    telem.submit = function(ping) {
      telem.submit = oldSubmit;
      resolve(ping);
    };
  });

  let { server, collection } = await prepareServer();

  Assert.equal(collection.count(), 0, "starting with 0 tab records");
  await engine.quickWrite();
  Assert.equal(collection.count(), 1, "tab record was written");

  let ping = await submitPromise;
  let syncs = ping.syncs;
  Assert.equal(syncs.length, 1);
  let sync = syncs[0];
  Assert.equal(sync.why, "quick-write");
  Assert.equal(sync.engines.length, 1);
  Assert.equal(sync.engines[0].name, "tabs");

  await promiseStopServer(server);
});
