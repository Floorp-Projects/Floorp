/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function makeSteamEngine() {
  return new SyncEngine('Steam', Service);
}

var server;

function test_url_attributes() {
  _("SyncEngine url attributes");
  let syncTesting = new SyncTestingInfrastructure(server);
  Service.clusterURL = "https://cluster/";
  let engine = makeSteamEngine();
  try {
    do_check_eq(engine.storageURL, "https://cluster/1.1/foo/storage/");
    do_check_eq(engine.engineURL, "https://cluster/1.1/foo/storage/steam");
    do_check_eq(engine.metaURL, "https://cluster/1.1/foo/storage/meta/global");
  } finally {
    Svc.Prefs.resetBranch("");
  }
}

function test_syncID() {
  _("SyncEngine.syncID corresponds to preference");
  let syncTesting = new SyncTestingInfrastructure(server);
  let engine = makeSteamEngine();
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
}

function test_lastSync() {
  _("SyncEngine.lastSync and SyncEngine.lastSyncLocal correspond to preferences");
  let syncTesting = new SyncTestingInfrastructure(server);
  let engine = makeSteamEngine();
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
}

function test_toFetch() {
  _("SyncEngine.toFetch corresponds to file on disk");
  let syncTesting = new SyncTestingInfrastructure(server);
  const filename = "weave/toFetch/steam.json";
  let engine = makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(engine.toFetch.length, 0);

    // Write file to disk
    let toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.toFetch = toFetch;
    do_check_eq(engine.toFetch, toFetch);
    // toFetch is written asynchronously
    engine._store._sleep(0);
    let fakefile = syncTesting.fakeFilesystem.fakeContents[filename];
    do_check_eq(fakefile, JSON.stringify(toFetch));

    // Read file from disk
    toFetch = [Utils.makeGUID(), Utils.makeGUID()];
    syncTesting.fakeFilesystem.fakeContents[filename] = JSON.stringify(toFetch);
    engine.loadToFetch();
    do_check_eq(engine.toFetch.length, 2);
    do_check_eq(engine.toFetch[0], toFetch[0]);
    do_check_eq(engine.toFetch[1], toFetch[1]);
  } finally {
    Svc.Prefs.resetBranch("");
  }
}

function test_previousFailed() {
  _("SyncEngine.previousFailed corresponds to file on disk");
  let syncTesting = new SyncTestingInfrastructure(server);
  const filename = "weave/failed/steam.json";
  let engine = makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(engine.previousFailed.length, 0);

    // Write file to disk
    let previousFailed = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.previousFailed = previousFailed;
    do_check_eq(engine.previousFailed, previousFailed);
    // previousFailed is written asynchronously
    engine._store._sleep(0);
    let fakefile = syncTesting.fakeFilesystem.fakeContents[filename];
    do_check_eq(fakefile, JSON.stringify(previousFailed));

    // Read file from disk
    previousFailed = [Utils.makeGUID(), Utils.makeGUID()];
    syncTesting.fakeFilesystem.fakeContents[filename] = JSON.stringify(previousFailed);
    engine.loadPreviousFailed();
    do_check_eq(engine.previousFailed.length, 2);
    do_check_eq(engine.previousFailed[0], previousFailed[0]);
    do_check_eq(engine.previousFailed[1], previousFailed[1]);
  } finally {
    Svc.Prefs.resetBranch("");
  }
}

function test_resetClient() {
  _("SyncEngine.resetClient resets lastSync and toFetch");
  let syncTesting = new SyncTestingInfrastructure(server);
  let engine = makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(Svc.Prefs.get("steam.lastSync"), undefined);
    do_check_eq(Svc.Prefs.get("steam.lastSyncLocal"), undefined);
    do_check_eq(engine.toFetch.length, 0);

    engine.lastSync = 123.45;
    engine.lastSyncLocal = 67890;
    engine.toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.previousFailed = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];

    engine.resetClient();
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.lastSyncLocal, 0);
    do_check_eq(engine.toFetch.length, 0);
    do_check_eq(engine.previousFailed.length, 0);
  } finally {
    Svc.Prefs.resetBranch("");
  }
}

function test_wipeServer() {
  _("SyncEngine.wipeServer deletes server data and resets the client.");
  let engine = makeSteamEngine();

  const PAYLOAD = 42;
  let steamCollection = new ServerWBO("steam", PAYLOAD);
  let server = httpd_setup({
    "/1.1/foo/storage/steam": steamCollection.handler()
  });
  let syncTesting = new SyncTestingInfrastructure(server);
  do_test_pending();

  try {
    // Some data to reset.
    engine.lastSync = 123.45;
    engine.toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];

    _("Wipe server data and reset client.");
    engine.wipeServer();
    do_check_eq(steamCollection.payload, undefined);
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.toFetch.length, 0);

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
  }
}

function run_test() {
  server = httpd_setup({});
  test_url_attributes();
  test_syncID();
  test_lastSync();
  test_toFetch();
  test_previousFailed();
  test_resetClient();
  test_wipeServer();

  server.stop(run_next_test);
}
