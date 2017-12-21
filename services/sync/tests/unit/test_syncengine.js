/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

async function makeSteamEngine() {
  let engine = new SyncEngine("Steam", Service);
  await engine.initialize();
  return engine;
}

async function testSteamEngineStorage(test) {
  try {
    let setupEngine = await makeSteamEngine();

    if (test.setup) {
      await test.setup(setupEngine);
    }

    // Finalize the engine to flush the backlog and previous failed to disk.
    await setupEngine.finalize();

    if (test.beforeCheck) {
      await test.beforeCheck();
    }

    let checkEngine = await makeSteamEngine();
    await test.check(checkEngine);

    await checkEngine.resetClient();
    await checkEngine.finalize();
  } finally {
    Svc.Prefs.resetBranch("");
  }
}

let server;

add_task(async function setup() {
  server = httpd_setup({});
});

add_task(async function test_url_attributes() {
  _("SyncEngine url attributes");
  await SyncTestingInfrastructure(server);
  Service.clusterURL = "https://cluster/1.1/foo/";
  let engine = await makeSteamEngine();
  try {
    Assert.equal(engine.storageURL, "https://cluster/1.1/foo/storage/");
    Assert.equal(engine.engineURL, "https://cluster/1.1/foo/storage/steam");
    Assert.equal(engine.metaURL, "https://cluster/1.1/foo/storage/meta/global");
  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_syncID() {
  _("SyncEngine.syncID corresponds to preference");
  await SyncTestingInfrastructure(server);
  let engine = await makeSteamEngine();
  try {
    // Ensure pristine environment
    Assert.equal(Svc.Prefs.get("steam.syncID"), undefined);

    // Performing the first get on the attribute will generate a new GUID.
    Assert.equal(engine.syncID, "fake-guid-00");
    Assert.equal(Svc.Prefs.get("steam.syncID"), "fake-guid-00");

    Svc.Prefs.set("steam.syncID", Utils.makeGUID());
    Assert.equal(Svc.Prefs.get("steam.syncID"), "fake-guid-01");
    Assert.equal(engine.syncID, "fake-guid-01");
  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_lastSync() {
  _("SyncEngine.lastSync and SyncEngine.lastSyncLocal correspond to preferences");
  await SyncTestingInfrastructure(server);
  let engine = await makeSteamEngine();
  try {
    // Ensure pristine environment
    Assert.equal(Svc.Prefs.get("steam.lastSync"), undefined);
    Assert.equal(engine.lastSync, 0);
    Assert.equal(Svc.Prefs.get("steam.lastSyncLocal"), undefined);
    Assert.equal(engine.lastSyncLocal, 0);

    // Floats are properly stored as floats and synced with the preference
    engine.lastSync = 123.45;
    Assert.equal(engine.lastSync, 123.45);
    Assert.equal(Svc.Prefs.get("steam.lastSync"), "123.45");

    // Integer is properly stored
    engine.lastSyncLocal = 67890;
    Assert.equal(engine.lastSyncLocal, 67890);
    Assert.equal(Svc.Prefs.get("steam.lastSyncLocal"), "67890");

    // resetLastSync() resets the value (and preference) to 0
    engine.resetLastSync();
    Assert.equal(engine.lastSync, 0);
    Assert.equal(Svc.Prefs.get("steam.lastSync"), "0");
  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_toFetch() {
  _("SyncEngine.toFetch corresponds to file on disk");
  await SyncTestingInfrastructure(server);
  const filename = "weave/toFetch/steam.json";

  await testSteamEngineStorage({
    toFetch: [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()],
    setup(engine) {
      // Ensure pristine environment
      Assert.equal(engine.toFetch.length, 0);

      // Write file to disk
      engine.toFetch = this.toFetch;
      Assert.equal(engine.toFetch, this.toFetch);
    },
    check(engine) {
      // toFetch is written asynchronously
      Assert.deepEqual(engine.toFetch, this.toFetch);
    },
  });

  await testSteamEngineStorage({
    toFetch: [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()],
    toFetch2: [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()],
    setup(engine) {
      // Make sure it work for consecutive writes before the callback is executed.
      engine.toFetch = this.toFetch;
      Assert.equal(engine.toFetch, this.toFetch);

      engine.toFetch = this.toFetch2;
      Assert.equal(engine.toFetch, this.toFetch2);
    },
    check(engine) {
      Assert.deepEqual(engine.toFetch, this.toFetch2);
    },
  });

  await testSteamEngineStorage({
    toFetch: [Utils.makeGUID(), Utils.makeGUID()],
    async beforeCheck() {
      let toFetchPath = OS.Path.join(OS.Constants.Path.profileDir, filename);
      let bytes = new TextEncoder().encode(JSON.stringify(this.toFetch));
      await OS.File.writeAtomic(toFetchPath, bytes,
                                { tmpPath: toFetchPath + ".tmp" });
    },
    check(engine) {
      // Read file from disk
      Assert.deepEqual(engine.toFetch, this.toFetch);
    },
  });
});

add_task(async function test_previousFailed() {
  _("SyncEngine.previousFailed corresponds to file on disk");
  await SyncTestingInfrastructure(server);
  const filename = "weave/failed/steam.json";

  await testSteamEngineStorage({
    previousFailed: [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()],
    setup(engine) {
      // Ensure pristine environment
      Assert.equal(engine.previousFailed.length, 0);

      // Write file to disk
      engine.previousFailed = this.previousFailed;
      Assert.equal(engine.previousFailed, this.previousFailed);
    },
    check(engine) {
      // previousFailed is written asynchronously
      Assert.deepEqual(engine.previousFailed, this.previousFailed);
    },
  });

  await testSteamEngineStorage({
    previousFailed: [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()],
    previousFailed2: [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()],
    setup(engine) {
      // Make sure it work for consecutive writes before the callback is executed.
      engine.previousFailed = this.previousFailed;
      Assert.equal(engine.previousFailed, this.previousFailed);

      engine.previousFailed = this.previousFailed2;
      Assert.equal(engine.previousFailed, this.previousFailed2);
    },
    check(engine) {
      Assert.deepEqual(engine.previousFailed, this.previousFailed2);
    },
  });

  await testSteamEngineStorage({
    previousFailed: [Utils.makeGUID(), Utils.makeGUID()],
    async beforeCheck() {
      let previousFailedPath = OS.Path.join(OS.Constants.Path.profileDir,
                                            filename);
      let bytes = new TextEncoder().encode(JSON.stringify(this.previousFailed));
      await OS.File.writeAtomic(previousFailedPath, bytes,
                                { tmpPath: previousFailedPath + ".tmp" });
    },
    check(engine) {
      // Read file from disk
      Assert.deepEqual(engine.previousFailed, this.previousFailed);
    },
  });
});

add_task(async function test_resetClient() {
  _("SyncEngine.resetClient resets lastSync and toFetch");
  await SyncTestingInfrastructure(server);
  let engine = await makeSteamEngine();
  try {
    // Ensure pristine environment
    Assert.equal(Svc.Prefs.get("steam.lastSync"), undefined);
    Assert.equal(Svc.Prefs.get("steam.lastSyncLocal"), undefined);
    Assert.equal(engine.toFetch.length, 0);

    engine.lastSync = 123.45;
    engine.lastSyncLocal = 67890;
    engine.toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.previousFailed = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];

    await engine.resetClient();
    Assert.equal(engine.lastSync, 0);
    Assert.equal(engine.lastSyncLocal, 0);
    Assert.equal(engine.toFetch.length, 0);
    Assert.equal(engine.previousFailed.length, 0);
  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_wipeServer() {
  _("SyncEngine.wipeServer deletes server data and resets the client.");
  let engine = await makeSteamEngine();

  const PAYLOAD = 42;
  let steamCollection = new ServerWBO("steam", PAYLOAD);
  let steamServer = httpd_setup({
    "/1.1/foo/storage/steam": steamCollection.handler()
  });
  await SyncTestingInfrastructure(steamServer);
  do_test_pending();

  try {
    // Some data to reset.
    engine.lastSync = 123.45;
    engine.toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];

    _("Wipe server data and reset client.");
    await engine.wipeServer();
    Assert.equal(steamCollection.payload, undefined);
    Assert.equal(engine.lastSync, 0);
    Assert.equal(engine.toFetch.length, 0);

  } finally {
    steamServer.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function finish() {
  await promiseStopServer(server);
});
