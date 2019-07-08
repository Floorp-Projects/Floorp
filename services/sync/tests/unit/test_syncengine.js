/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

async function makeSteamEngine() {
  let engine = new SyncEngine("Steam", Service);
  await engine.initialize();
  return engine;
}

function guidSetOfSize(length) {
  return new SerializableSet(Array.from({ length }, () => Utils.makeGUID()));
}

function assertSetsEqual(a, b) {
  // Assert.deepEqual doesn't understand Set.
  Assert.deepEqual(Array.from(a).sort(), Array.from(b).sort());
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
    Assert.equal(await engine.getSyncID(), "");

    // Performing the first get on the attribute will generate a new GUID.
    Assert.equal(await engine.resetLocalSyncID(), "fake-guid-00");
    Assert.equal(Svc.Prefs.get("steam.syncID"), "fake-guid-00");

    Svc.Prefs.set("steam.syncID", Utils.makeGUID());
    Assert.equal(Svc.Prefs.get("steam.syncID"), "fake-guid-01");
    Assert.equal(await engine.getSyncID(), "fake-guid-01");
  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_lastSync() {
  _("SyncEngine.lastSync corresponds to preferences");
  await SyncTestingInfrastructure(server);
  let engine = await makeSteamEngine();
  try {
    // Ensure pristine environment
    Assert.equal(Svc.Prefs.get("steam.lastSync"), undefined);
    Assert.equal(await engine.getLastSync(), 0);

    // Floats are properly stored as floats and synced with the preference
    await engine.setLastSync(123.45);
    Assert.equal(await engine.getLastSync(), 123.45);
    Assert.equal(Svc.Prefs.get("steam.lastSync"), "123.45");

    // Integer is properly stored
    await engine.setLastSync(67890);
    Assert.equal(await engine.getLastSync(), 67890);
    Assert.equal(Svc.Prefs.get("steam.lastSync"), "67890");

    // resetLastSync() resets the value (and preference) to 0
    await engine.resetLastSync();
    Assert.equal(await engine.getLastSync(), 0);
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
    toFetch: guidSetOfSize(3),
    setup(engine) {
      // Ensure pristine environment
      Assert.equal(engine.toFetch.size, 0);

      // Write file to disk
      engine.toFetch = this.toFetch;
      Assert.equal(engine.toFetch, this.toFetch);
    },
    check(engine) {
      // toFetch is written asynchronously
      assertSetsEqual(engine.toFetch, this.toFetch);
    },
  });

  await testSteamEngineStorage({
    toFetch: guidSetOfSize(4),
    toFetch2: guidSetOfSize(5),
    setup(engine) {
      // Make sure it work for consecutive writes before the callback is executed.
      engine.toFetch = this.toFetch;
      Assert.equal(engine.toFetch, this.toFetch);

      engine.toFetch = this.toFetch2;
      Assert.equal(engine.toFetch, this.toFetch2);
    },
    check(engine) {
      assertSetsEqual(engine.toFetch, this.toFetch2);
    },
  });

  await testSteamEngineStorage({
    toFetch: guidSetOfSize(2),
    async beforeCheck() {
      let toFetchPath = OS.Path.join(OS.Constants.Path.profileDir, filename);
      let bytes = new TextEncoder().encode(JSON.stringify(this.toFetch));
      await OS.File.writeAtomic(toFetchPath, bytes, {
        tmpPath: toFetchPath + ".tmp",
      });
    },
    check(engine) {
      // Read file from disk
      assertSetsEqual(engine.toFetch, this.toFetch);
    },
  });
});

add_task(async function test_previousFailed() {
  _("SyncEngine.previousFailed corresponds to file on disk");
  await SyncTestingInfrastructure(server);
  const filename = "weave/failed/steam.json";

  await testSteamEngineStorage({
    previousFailed: guidSetOfSize(3),
    setup(engine) {
      // Ensure pristine environment
      Assert.equal(engine.previousFailed.size, 0);

      // Write file to disk
      engine.previousFailed = this.previousFailed;
      Assert.equal(engine.previousFailed, this.previousFailed);
    },
    check(engine) {
      // previousFailed is written asynchronously
      assertSetsEqual(engine.previousFailed, this.previousFailed);
    },
  });

  await testSteamEngineStorage({
    previousFailed: guidSetOfSize(4),
    previousFailed2: guidSetOfSize(5),
    setup(engine) {
      // Make sure it work for consecutive writes before the callback is executed.
      engine.previousFailed = this.previousFailed;
      Assert.equal(engine.previousFailed, this.previousFailed);

      engine.previousFailed = this.previousFailed2;
      Assert.equal(engine.previousFailed, this.previousFailed2);
    },
    check(engine) {
      assertSetsEqual(engine.previousFailed, this.previousFailed2);
    },
  });

  await testSteamEngineStorage({
    previousFailed: guidSetOfSize(2),
    async beforeCheck() {
      let previousFailedPath = OS.Path.join(
        OS.Constants.Path.profileDir,
        filename
      );
      let bytes = new TextEncoder().encode(JSON.stringify(this.previousFailed));
      await OS.File.writeAtomic(previousFailedPath, bytes, {
        tmpPath: previousFailedPath + ".tmp",
      });
    },
    check(engine) {
      // Read file from disk
      assertSetsEqual(engine.previousFailed, this.previousFailed);
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
    Assert.equal(engine.toFetch.size, 0);

    await engine.setLastSync(123.45);
    engine.toFetch = guidSetOfSize(4);
    engine.previousFailed = guidSetOfSize(3);

    await engine.resetClient();
    Assert.equal(await engine.getLastSync(), 0);
    Assert.equal(engine.toFetch.size, 0);
    Assert.equal(engine.previousFailed.size, 0);
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
    "/1.1/foo/storage/steam": steamCollection.handler(),
  });
  await SyncTestingInfrastructure(steamServer);
  do_test_pending();

  try {
    // Some data to reset.
    await engine.setLastSync(123.45);
    engine.toFetch = guidSetOfSize(3);

    _("Wipe server data and reset client.");
    await engine.wipeServer();
    Assert.equal(steamCollection.payload, undefined);
    Assert.equal(await engine.getLastSync(), 0);
    Assert.equal(engine.toFetch.size, 0);
  } finally {
    steamServer.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function finish() {
  await promiseStopServer(server);
});
