Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");

function makeSteamEngine() {
  return new SyncEngine('Steam');
}

var syncTesting = new SyncTestingInfrastructure(makeSteamEngine);

function test_url_attributes() {
  _("SyncEngine url attributes");
  Svc.Prefs.set("clusterURL", "https://cluster/");
  let engine = makeSteamEngine();
  try {
    do_check_eq(engine.storageURL, "https://cluster/1.0/foo/storage/");
    do_check_eq(engine.engineURL, "https://cluster/1.0/foo/storage/steam");
    do_check_eq(engine.cryptoMetaURL,
                "https://cluster/1.0/foo/storage/crypto/steam");
    do_check_eq(engine.metaURL, "https://cluster/1.0/foo/storage/meta/global");
  } finally {
    Svc.Prefs.resetBranch("");
  }
}

function test_syncID() {
  _("SyncEngine.syncID corresponds to preference");
  let engine = makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(Svc.Prefs.get("steam.syncID"), undefined);

    // Performing the first get on the attribute will generate a new GUID.
    do_check_eq(engine.syncID, "fake-guid-0");
    do_check_eq(Svc.Prefs.get("steam.syncID"), "fake-guid-0");

    Svc.Prefs.set("steam.syncID", Utils.makeGUID());
    do_check_eq(Svc.Prefs.get("steam.syncID"), "fake-guid-1");
    do_check_eq(engine.syncID, "fake-guid-1");
  } finally {
    Svc.Prefs.resetBranch("");
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}

function test_lastSync() {
  _("SyncEngine.lastSync corresponds to preference and stores floats");
  let engine = makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(Svc.Prefs.get("steam.lastSync"), undefined);
    do_check_eq(engine.lastSync, 0);

    // Floats are properly stored as floats and synced with the preference
    engine.lastSync = 123.45;
    do_check_eq(engine.lastSync, 123.45);
    do_check_eq(Svc.Prefs.get("steam.lastSync"), "123.45");

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
  let engine = makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(engine.toFetch.length, 0);

    // Write file to disk
    let toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.toFetch = toFetch;
    do_check_eq(engine.toFetch, toFetch);
    let fakefile = syncTesting.fakeFilesystem.fakeContents[
        "weave/toFetch/steam.json"];
    do_check_eq(fakefile, JSON.stringify(toFetch));

    // Read file from disk
    toFetch = [Utils.makeGUID(), Utils.makeGUID()];
    syncTesting.fakeFilesystem.fakeContents["weave/toFetch/steam.json"]
        = JSON.stringify(toFetch);
    engine.loadToFetch();
    do_check_eq(engine.toFetch.length, 2);
    do_check_eq(engine.toFetch[0], toFetch[0]);
    do_check_eq(engine.toFetch[1], toFetch[1]);
  } finally {
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
  }
}

function test_resetClient() {
  _("SyncEngine.resetClient resets lastSync and toFetch");
  let engine = makeSteamEngine();
  try {
    // Ensure pristine environment
    do_check_eq(Svc.Prefs.get("steam.lastSync"), undefined);
    do_check_eq(engine.toFetch.length, 0);

    engine.toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.lastSync = 123.45;

    engine.resetClient();
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.toFetch.length, 0);
  } finally {
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
    Svc.Prefs.resetBranch("");
  }
}

function test_wipeServer() {
  _("SyncEngine.wipeServer deletes server data and resets the client.");
  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  let engine = makeSteamEngine();

  const PAYLOAD = 42;
  let steamCrypto = new ServerWBO("steam", PAYLOAD);
  let steamCollection = new ServerWBO("steam", PAYLOAD);
  let server = httpd_setup({
    "/1.0/foo/storage/crypto/steam": steamCrypto.handler(),
    "/1.0/foo/storage/steam": steamCollection.handler()
  });
  do_test_pending();

  try {
    // Some data to reset.
    engine.toFetch = [Utils.makeGUID(), Utils.makeGUID(), Utils.makeGUID()];
    engine.lastSync = 123.45;

    _("Wipe server data and reset client.");
    engine.wipeServer(true);
    do_check_eq(steamCollection.payload, undefined);
    do_check_eq(engine.lastSync, 0);
    do_check_eq(engine.toFetch.length, 0);

    _("We passed a truthy arg earlier in which case it doesn't wipe the crypto collection.");
    do_check_eq(steamCrypto.payload, PAYLOAD);
    engine.wipeServer();
    do_check_eq(steamCrypto.payload, undefined);

  } finally {
    server.stop(do_test_finished);
    syncTesting = new SyncTestingInfrastructure(makeSteamEngine);
    Svc.Prefs.resetBranch("");
  }
}

function run_test() {
  test_url_attributes();
  test_syncID();
  test_lastSync();
  test_toFetch();
  test_resetClient();
  test_wipeServer();
}
