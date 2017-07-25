/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function QuietStore() {
  Store.call("Quiet");
}
QuietStore.prototype = {
  async getAllIDs() {
    return [];
  }
}

function SteamEngine() {
  SyncEngine.call(this, "Steam", Service);
}
SteamEngine.prototype = {
  __proto__: SyncEngine.prototype,
  // We're not interested in engine sync but what the service does.
  _storeObj: QuietStore,

  _sync: async function _sync() {
    await this._syncStartup();
  }
};

function StirlingEngine() {
  SyncEngine.call(this, "Stirling", Service);
}
StirlingEngine.prototype = {
  __proto__: SteamEngine.prototype,
  // This engine's enabled state is the same as the SteamEngine's.
  get prefName() {
    return "steam";
  }
};

// Tracking info/collections.
var collectionsHelper = track_collections_helper();
var upd = collectionsHelper.with_updated_collection;

function sync_httpd_setup(handlers) {

  handlers["/1.1/johndoe/info/collections"] = collectionsHelper.handler;
  delete collectionsHelper.collections.crypto;
  delete collectionsHelper.collections.meta;

  let cr = new ServerWBO("keys");
  handlers["/1.1/johndoe/storage/crypto/keys"] =
    upd("crypto", cr.handler());

  let cl = new ServerCollection();
  handlers["/1.1/johndoe/storage/clients"] =
    upd("clients", cl.handler());

  return httpd_setup(handlers);
}

async function setUp(server) {
  await SyncTestingInfrastructure(server, "johndoe", "ilovejane");
  // Ensure that the server has valid keys so that logging in will work and not
  // result in a server wipe, rendering many of these tests useless.
  generateNewKeys(Service.collectionKeys);
  let serverKeys = Service.collectionKeys.asWBO("crypto", "keys");
  serverKeys.encrypt(Service.identity.syncKeyBundle);
  return serverKeys.upload(Service.resource(Service.cryptoKeysURL)).success;
}

const PAYLOAD = 42;

add_task(async function setup() {
  initTestLogging();
  Service.engineManager.clear();

  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Service").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.ErrorHandler").level = Log.Level.Trace;
  validate_all_future_pings();

  await Service.engineManager.register(SteamEngine);
  await Service.engineManager.register(StirlingEngine);
});

add_task(async function test_newAccount() {
  enableValidationPrefs();

  _("Test: New account does not disable locally enabled engines.");
  let engine = Service.engineManager.get("steam");
  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global": new ServerWBO("global", {}).handler(),
    "/1.1/johndoe/storage/steam": new ServerWBO("steam", {}).handler()
  });
  await setUp(server);

  try {
    _("Engine is enabled from the beginning.");
    Service._ignorePrefObserver = true;
    engine.enabled = true;
    Service._ignorePrefObserver = false;

    _("Sync.");
    await Service.sync();

    _("Engine continues to be enabled.");
    do_check_true(engine.enabled);
  } finally {
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_enabledLocally() {
  enableValidationPrefs();

  _("Test: Engine is disabled on remote clients and enabled locally");
  Service.syncID = "abcdefghij";
  let engine = Service.engineManager.get("steam");
  let metaWBO = new ServerWBO("global", {syncID: Service.syncID,
                                         storageVersion: STORAGE_VERSION,
                                         engines: {}});
  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.1/johndoe/storage/steam": new ServerWBO("steam", {}).handler()
  });
  await setUp(server);

  try {
    _("Enable engine locally.");
    engine.enabled = true;

    _("Sync.");
    await Service.sync();

    _("Meta record now contains the new engine.");
    do_check_true(!!metaWBO.data.engines.steam);

    _("Engine continues to be enabled.");
    do_check_true(engine.enabled);
  } finally {
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_disabledLocally() {
  enableValidationPrefs();

  _("Test: Engine is enabled on remote clients and disabled locally");
  Service.syncID = "abcdefghij";
  let engine = Service.engineManager.get("steam");
  let metaWBO = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {steam: {syncID: engine.syncID,
                      version: engine.version}}
  });
  let steamCollection = new ServerWBO("steam", PAYLOAD);

  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.1/johndoe/storage/steam": steamCollection.handler()
  });
  await setUp(server);

  try {
    _("Disable engine locally.");
    Service._ignorePrefObserver = true;
    engine.enabled = true;
    Service._ignorePrefObserver = false;
    engine.enabled = false;

    _("Sync.");
    await Service.sync();

    _("Meta record no longer contains engine.");
    do_check_false(!!metaWBO.data.engines.steam);

    _("Server records are wiped.");
    do_check_eq(steamCollection.payload, undefined);

    _("Engine continues to be disabled.");
    do_check_false(engine.enabled);
  } finally {
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_disabledLocally_wipe503() {
  enableValidationPrefs();

  _("Test: Engine is enabled on remote clients and disabled locally");
  Service.syncID = "abcdefghij";
  let engine = Service.engineManager.get("steam");
  let metaWBO = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {steam: {syncID: engine.syncID,
                      version: engine.version}}
  });

  function service_unavailable(request, response) {
    let body = "Service Unavailable";
    response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
    response.setHeader("Retry-After", "23");
    response.bodyOutputStream.write(body, body.length);
  }

  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.1/johndoe/storage/steam": service_unavailable
  });
  await setUp(server);

  _("Disable engine locally.");
  Service._ignorePrefObserver = true;
  engine.enabled = true;
  Service._ignorePrefObserver = false;
  engine.enabled = false;

  let promiseObserved = promiseOneObserver("weave:ui:sync:error");

  _("Sync.");
  Service.errorHandler.syncAndReportErrors();
  await promiseObserved;
  do_check_eq(Service.status.sync, SERVER_MAINTENANCE);

  await Service.startOver();
  await promiseStopServer(server);
});

add_task(async function test_enabledRemotely() {
  enableValidationPrefs();

  _("Test: Engine is disabled locally and enabled on a remote client");
  Service.syncID = "abcdefghij";
  let engine = Service.engineManager.get("steam");
  let metaWBO = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {steam: {syncID: engine.syncID,
                      version: engine.version}}
  });
  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global":
    upd("meta", metaWBO.handler()),

    "/1.1/johndoe/storage/steam":
    upd("steam", new ServerWBO("steam", {}).handler())
  });
  await setUp(server);

  // We need to be very careful how we do this, so that we don't trigger a
  // fresh start!
  try {
    _("Upload some keys to avoid a fresh start.");
    let wbo = Service.collectionKeys.generateNewKeysWBO();
    wbo.encrypt(Service.identity.syncKeyBundle);
    do_check_eq(200, (await wbo.upload(Service.resource(Service.cryptoKeysURL))).status);

    _("Engine is disabled.");
    do_check_false(engine.enabled);

    _("Sync.");
    await Service.sync();

    _("Engine is enabled.");
    do_check_true(engine.enabled);

    _("Meta record still present.");
    do_check_eq(metaWBO.data.engines.steam.syncID, engine.syncID);
  } finally {
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_disabledRemotelyTwoClients() {
  enableValidationPrefs();

  _("Test: Engine is enabled locally and disabled on a remote client... with two clients.");
  Service.syncID = "abcdefghij";
  let engine = Service.engineManager.get("steam");
  let metaWBO = new ServerWBO("global", {syncID: Service.syncID,
                                         storageVersion: STORAGE_VERSION,
                                         engines: {}});
  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global":
    upd("meta", metaWBO.handler()),

    "/1.1/johndoe/storage/steam":
    upd("steam", new ServerWBO("steam", {}).handler())
  });
  await setUp(server);

  try {
    _("Enable engine locally.");
    Service._ignorePrefObserver = true;
    engine.enabled = true;
    Service._ignorePrefObserver = false;

    _("Sync.");
    await Service.sync();

    _("Disable engine by deleting from meta/global.");
    let d = metaWBO.data;
    delete d.engines.steam;
    metaWBO.payload = JSON.stringify(d);
    metaWBO.modified = Date.now() / 1000;

    _("Add a second client and verify that the local pref is changed.");
    Service.clientsEngine._store._remoteClients.foobar = {name: "foobar", type: "desktop"};
    await Service.sync();

    _("Engine is disabled.");
    do_check_false(engine.enabled);

  } finally {
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_disabledRemotely() {
  enableValidationPrefs();

  _("Test: Engine is enabled locally and disabled on a remote client");
  Service.syncID = "abcdefghij";
  let engine = Service.engineManager.get("steam");
  let metaWBO = new ServerWBO("global", {syncID: Service.syncID,
                                         storageVersion: STORAGE_VERSION,
                                         engines: {}});
  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.1/johndoe/storage/steam": new ServerWBO("steam", {}).handler()
  });
  await setUp(server);

  try {
    _("Enable engine locally.");
    Service._ignorePrefObserver = true;
    engine.enabled = true;
    Service._ignorePrefObserver = false;

    _("Sync.");
    await Service.sync();

    _("Engine is not disabled: only one client.");
    do_check_true(engine.enabled);

  } finally {
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_dependentEnginesEnabledLocally() {
  enableValidationPrefs();

  _("Test: Engine is disabled on remote clients and enabled locally");
  Service.syncID = "abcdefghij";
  let steamEngine = Service.engineManager.get("steam");
  let stirlingEngine = Service.engineManager.get("stirling");
  let metaWBO = new ServerWBO("global", {syncID: Service.syncID,
                                         storageVersion: STORAGE_VERSION,
                                         engines: {}});
  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.1/johndoe/storage/steam": new ServerWBO("steam", {}).handler(),
    "/1.1/johndoe/storage/stirling": new ServerWBO("stirling", {}).handler()
  });
  await setUp(server);

  try {
    _("Enable engine locally. Doing it on one is enough.");
    steamEngine.enabled = true;

    _("Sync.");
    await Service.sync();

    _("Meta record now contains the new engines.");
    do_check_true(!!metaWBO.data.engines.steam);
    do_check_true(!!metaWBO.data.engines.stirling);

    _("Engines continue to be enabled.");
    do_check_true(steamEngine.enabled);
    do_check_true(stirlingEngine.enabled);
  } finally {
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_dependentEnginesDisabledLocally() {
  enableValidationPrefs();

  _("Test: Two dependent engines are enabled on remote clients and disabled locally");
  Service.syncID = "abcdefghij";
  let steamEngine = Service.engineManager.get("steam");
  let stirlingEngine = Service.engineManager.get("stirling");
  let metaWBO = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {steam: {syncID: steamEngine.syncID,
                      version: steamEngine.version},
              stirling: {syncID: stirlingEngine.syncID,
                         version: stirlingEngine.version}}
  });

  let steamCollection = new ServerWBO("steam", PAYLOAD);
  let stirlingCollection = new ServerWBO("stirling", PAYLOAD);

  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global":     metaWBO.handler(),
    "/1.1/johndoe/storage/steam":           steamCollection.handler(),
    "/1.1/johndoe/storage/stirling":        stirlingCollection.handler()
  });
  await setUp(server);

  try {
    _("Disable engines locally. Doing it on one is enough.");
    Service._ignorePrefObserver = true;
    steamEngine.enabled = true;
    do_check_true(stirlingEngine.enabled);
    Service._ignorePrefObserver = false;
    steamEngine.enabled = false;
    do_check_false(stirlingEngine.enabled);

    _("Sync.");
    await Service.sync();

    _("Meta record no longer contains engines.");
    do_check_false(!!metaWBO.data.engines.steam);
    do_check_false(!!metaWBO.data.engines.stirling);

    _("Server records are wiped.");
    do_check_eq(steamCollection.payload, undefined);
    do_check_eq(stirlingCollection.payload, undefined);

    _("Engines continue to be disabled.");
    do_check_false(steamEngine.enabled);
    do_check_false(stirlingEngine.enabled);
  } finally {
    await Service.startOver();
    await promiseStopServer(server);
  }
});
