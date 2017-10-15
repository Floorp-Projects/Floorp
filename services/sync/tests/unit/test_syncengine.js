/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

async function makeSteamEngine() {
  let engine = new SyncEngine("Steam", Service);
  await engine.initialize();
  return engine;
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
    do_check_eq(engine.storageURL, "https://cluster/1.1/foo/storage/");
    do_check_eq(engine.engineURL, "https://cluster/1.1/foo/storage/steam");
    do_check_eq(engine.metaURL, "https://cluster/1.1/foo/storage/meta/global");
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
    do_check_eq(Svc.Prefs.get("steam.syncID"), undefined);

    // Performing the first get on the attribute will generate a new GUID.
    do_check_eq(engine.syncID, "fake-guid-00");
    do_check_eq(Svc.Prefs.get("steam.syncID"), "fake-guid-00");

    Svc.Prefs.set("steam.syncID", Utils.makeGUID());
    do_check_eq(Svc.Prefs.get("steam.syncID"), "fake-guid-01");
    do_check_eq(engine.syncID, "fake-guid-01");
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
    do_check_eq(Svc.Prefs.get("steam.lastSync"), undefined);
    do_check_eq(engine.lastSync, 0);
    do_check_eq(Svc.Prefs.get("steam.lastSyncLocal"), undefined);
    do_check_eq(engine.lastSyncLocal, 0);

    // Floats are properly stored as floats and synced with the preference
    engine.lastSync = 123.45;
    do_check_eq(engine.lastSync, 123.45);
    do_check_eq(Svc.Prefs.get("steam.lastSync"), "123.45");

    // Integer is properly stored
    engine.lastSyncLocal = 67890;
    do_check_eq(engine.lastSyncLocal, 67890);
    do_check_eq(Svc.Prefs.get("steam.lastSyncLocal"), "67890");

    // resetLastSync() resets the value (and preference) to 0
    engine.resetLastSync();
    do_check_eq(engine.lastSync, 0);
    do_check_eq(Svc.Prefs.get("steam.lastSync"), "0");
  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_toFetch() {
  _("SyncEngine.toFetch corresponds to file on disk");
  let syncTesting = await SyncTestingInfrastructure(server);
  const filename = "weave/toFetch/steam.json";
  let engine = await makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(engine.toFetch.length, 0);

    // Write file to disk
    let toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    let wrotePromise = promiseOneObserver("sync-testing:file-saved:toFetch");
    engine.toFetch = toFetch;
    do_check_eq(engine.toFetch, toFetch);
    // toFetch is written asynchronously
    await wrotePromise;
    let fakefile = syncTesting.fakeFilesystem.fakeContents[filename];
    do_check_eq(fakefile, JSON.stringify(toFetch));

    // Make sure it work for consecutive writes before the callback is executed.
    toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    let toFetch2 = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    wrotePromise = promiseOneObserver("sync-testing:file-saved:toFetch");

    engine.toFetch = toFetch;
    do_check_eq(engine.toFetch, toFetch);

    engine.toFetch = toFetch2;
    do_check_eq(engine.toFetch, toFetch2);
    // Note that do to the way CommonUtils.namedTimer works, we won't get a 2nd callback.
    await wrotePromise;
    fakefile = syncTesting.fakeFilesystem.fakeContents[filename];
    do_check_eq(fakefile, JSON.stringify(toFetch2));

    // Read file from disk
    toFetch = [Utils.makeGUID(), Utils.makeGUID()];
    syncTesting.fakeFilesystem.fakeContents[filename] = JSON.stringify(toFetch);
    await engine.loadToFetch();
    do_check_eq(engine.toFetch.length, 2);
    do_check_eq(engine.toFetch[0], toFetch[0]);
    do_check_eq(engine.toFetch[1], toFetch[1]);
  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_previousFailed() {
  _("SyncEngine.previousFailed corresponds to file on disk");
  let syncTesting = await SyncTestingInfrastructure(server);
  const filename = "weave/failed/steam.json";
  let engine = await makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(engine.previousFailed.length, 0);

    // Write file to disk
    let previousFailed = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    let wrotePromise = promiseOneObserver("sync-testing:file-saved:previousFailed");
    engine.previousFailed = previousFailed;
    do_check_eq(engine.previousFailed, previousFailed);
    // previousFailed is written asynchronously
    await wrotePromise;
    let fakefile = syncTesting.fakeFilesystem.fakeContents[filename];
    do_check_eq(fakefile, JSON.stringify(previousFailed));

    // Make sure it work for consecutive writes before the callback is executed.
    previousFailed = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    let previousFailed2 = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    wrotePromise = promiseOneObserver("sync-testing:file-saved:previousFailed");

    engine.previousFailed = previousFailed;
    do_check_eq(engine.previousFailed, previousFailed);

    engine.previousFailed = previousFailed2;
    do_check_eq(engine.previousFailed, previousFailed2);
    // Note that do to the way CommonUtils.namedTimer works, we're only notified once.
    await wrotePromise;
    fakefile = syncTesting.fakeFilesystem.fakeContents[filename];
    do_check_eq(fakefile, JSON.stringify(previousFailed2));

    // Read file from disk
    previousFailed = [Utils.makeGUID(), Utils.makeGUID()];
    syncTesting.fakeFilesystem.fakeContents[filename] = JSON.stringify(previousFailed);
    await engine.loadPreviousFailed();
    do_check_eq(engine.previousFailed.length, 2);
    do_check_eq(engine.previousFailed[0], previousFailed[0]);
    do_check_eq(engine.previousFailed[1], previousFailed[1]);
  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_resetClient() {
  _("SyncEngine.resetClient resets lastSync and toFetch");
  await SyncTestingInfrastructure(server);
  let engine = await makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(Svc.Prefs.get("steam.lastSync"), undefined);
    do_check_eq(Svc.Prefs.get("steam.lastSyncLocal"), undefined);
    do_check_eq(engine.toFetch.length, 0);

    engine.lastSync = 123.45;
    engine.lastSyncLocal = 67890;
    engine.toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.previousFailed = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];

    await engine.resetClient();
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.lastSyncLocal, 0);
    do_check_eq(engine.toFetch.length, 0);
    do_check_eq(engine.previousFailed.length, 0);
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
    do_check_eq(steamCollection.payload, undefined);
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.toFetch.length, 0);

  } finally {
    steamServer.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function finish() {
  await promiseStopServer(server);
});
