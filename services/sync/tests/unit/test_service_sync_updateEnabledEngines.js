Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/base_records/crypto.js");
Cu.import("resource://services-sync/base_records/wbo.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");

initTestLogging();

function QuietStore() {
  Store.call("Quiet");
}
QuietStore.prototype = {
  getAllIDs: function getAllIDs() {
    return [];
  }
}

function SteamEngine() {
  SyncEngine.call(this, "Steam");
}
SteamEngine.prototype = {
  __proto__: SyncEngine.prototype,
  // We're not interested in engine sync but what the service does.
  _storeObj: QuietStore,
  
  _sync: function _sync() {
    this._syncStartup();
  }
};
Engines.register(SteamEngine);

function StirlingEngine() {
  SyncEngine.call(this, "Stirling");
}
StirlingEngine.prototype = {
  __proto__: SteamEngine.prototype,
  // This engine's enabled state is the same as the SteamEngine's.
  get prefName() "steam"
};
Engines.register(StirlingEngine);

// Tracking info/collections.
let collectionsHelper = track_collections_helper();
let upd = collectionsHelper.with_updated_collection;

function sync_httpd_setup(handlers) {
    
  handlers["/1.0/johndoe/info/collections"] = collectionsHelper.handler;
  
  let cr = new ServerWBO("keys");
  handlers["/1.0/johndoe/storage/crypto/keys"] =
    upd("crypto", cr.handler());
  
  let cl = new ServerCollection();
  handlers["/1.0/johndoe/storage/clients"] =
    upd("clients", cl.handler());
  
  return httpd_setup(handlers);
}

function setUp() {
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "sekrit";
  Service.clusterURL = "http://localhost:8080/";
  new FakeCryptoService();
}

const PAYLOAD = 42;

function test_newAccount() {
  _("Test: New account does not disable locally enabled engines.");
  let engine = Engines.get("steam");
  let server = sync_httpd_setup({
    "/1.0/johndoe/storage/meta/global": new ServerWBO("global", {}).handler(),
    "/1.0/johndoe/storage/steam": new ServerWBO("steam", {}).handler()
  });
  do_test_pending();
  setUp();

  try {
    _("Engine is enabled from the beginning.");
    Service._ignorePrefObserver = true;
    engine.enabled = true;
    Service._ignorePrefObserver = false;

    _("Sync.");
    Weave.Service.login();
    Weave.Service.sync();

    _("Engine continues to be enabled.");
    do_check_true(engine.enabled);
  } finally {
    server.stop(do_test_finished);
    Service.startOver();
  }
}

function test_enabledLocally() {
  _("Test: Engine is disabled on remote clients and enabled locally");
  Service.syncID = "abcdefghij";
  let engine = Engines.get("steam");
  let metaWBO = new ServerWBO("global", {syncID: Service.syncID,
                                         storageVersion: STORAGE_VERSION,
                                         engines: {}});
  let server = sync_httpd_setup({
    "/1.0/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.0/johndoe/storage/steam": new ServerWBO("steam", {}).handler()
  });
  do_test_pending();
  setUp();

  try {
    _("Enable engine locally.");
    engine.enabled = true;

    _("Sync.");
    Weave.Service.login();
    Weave.Service.sync();

    _("Meta record now contains the new engine.");
    do_check_true(!!metaWBO.data.engines.steam);

    _("Engine continues to be enabled.");
    do_check_true(engine.enabled);
  } finally {
    server.stop(do_test_finished);
    Service.startOver();
  }
}

function test_disabledLocally() {
  _("Test: Engine is enabled on remote clients and disabled locally");
  Service.syncID = "abcdefghij";
  let engine = Engines.get("steam");
  let metaWBO = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {steam: {syncID: engine.syncID,
                      version: engine.version}}
  });
  let steamCollection = new ServerWBO("steam", PAYLOAD);
  let server = sync_httpd_setup({
    "/1.0/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.0/johndoe/storage/steam": steamCollection.handler()
  });
  do_test_pending();
  setUp();

  try {
    _("Disable engine locally.");
    Service._ignorePrefObserver = true;
    engine.enabled = true;
    Service._ignorePrefObserver = false;
    engine.enabled = false;

    _("Sync.");
    Weave.Service.login();
    Weave.Service.sync();

    _("Meta record no longer contains engine.");
    do_check_false(!!metaWBO.data.engines.steam);

    _("Server records are wiped.");
    do_check_eq(steamCollection.payload, undefined);

    _("Engine continues to be disabled.");
    do_check_false(engine.enabled);
  } finally {
    server.stop(do_test_finished);
    Service.startOver();
  }
}

function test_enabledRemotely() {
  _("Test: Engine is disabled locally and enabled on a remote client");
  Service.syncID = "abcdefghij";
  let engine = Engines.get("steam");
  let metaWBO = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {steam: {syncID: engine.syncID,
                      version: engine.version}}
  });
  let server = sync_httpd_setup({
    "/1.0/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.0/johndoe/storage/steam": new ServerWBO("steam", {}).handler()
  });
  do_test_pending();
  setUp();

  try {
    _("Engine is disabled.");
    do_check_false(engine.enabled);

    _("Sync.");
    Weave.Service.login();
    Weave.Service.sync();

    _("Engine is enabled.");
    do_check_true(engine.enabled);

    _("Meta record still present.");
    do_check_eq(metaWBO.data.engines.steam.syncID, engine.syncID);
  } finally {
    server.stop(do_test_finished);
    Service.startOver();
  }
}

function test_disabledRemotelyTwoClients() {
  _("Test: Engine is enabled locally and disabled on a remote client... with two clients.");
  Service.syncID = "abcdefghij";
  let engine = Engines.get("steam");
  let metaWBO = new ServerWBO("global", {syncID: Service.syncID,
                                         storageVersion: STORAGE_VERSION,
                                         engines: {}});
  let server = sync_httpd_setup({
    "/1.0/johndoe/storage/meta/global":
    upd("meta", metaWBO.handler()),
      
    "/1.0/johndoe/storage/steam":
    upd("steam", new ServerWBO("steam", {}).handler())
  });
  do_test_pending();
  setUp();

  try {
    _("Enable engine locally.");
    Service._ignorePrefObserver = true;
    engine.enabled = true;
    Service._ignorePrefObserver = false;

    _("Sync.");
    Weave.Service.login();
    Weave.Service.sync();

    _("Disable engine by deleting from meta/global.");
    let d = metaWBO.data; 
    delete d.engines["steam"];
    metaWBO.payload = JSON.stringify(d);
    metaWBO.modified = Date.now() / 1000;
    
    _("Add a second client and verify that the local pref is changed.");
    Clients._store._remoteClients["foobar"] = {name: "foobar", type: "desktop"};
    Weave.Service.sync();
    
    _("Engine is disabled.");
    do_check_false(engine.enabled);
    
  } finally {
    server.stop(do_test_finished);
    Service.startOver();
  }
}

function test_disabledRemotely() {
  _("Test: Engine is enabled locally and disabled on a remote client");
  Service.syncID = "abcdefghij";
  let engine = Engines.get("steam");
  let metaWBO = new ServerWBO("global", {syncID: Service.syncID,
                                         storageVersion: STORAGE_VERSION,
                                         engines: {}});
  let server = sync_httpd_setup({
    "/1.0/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.0/johndoe/storage/steam": new ServerWBO("steam", {}).handler()
  });
  do_test_pending();
  setUp();

  try {
    _("Enable engine locally.");
    Service._ignorePrefObserver = true;
    engine.enabled = true;
    Service._ignorePrefObserver = false;

    _("Sync.");
    Weave.Service.login();
    Weave.Service.sync();

    _("Engine is not disabled: only one client.");
    do_check_true(engine.enabled);
    
  } finally {
    server.stop(do_test_finished);
    Service.startOver();
  }
}

function test_dependentEnginesEnabledLocally() {
  _("Test: Engine is disabled on remote clients and enabled locally");
  Service.syncID = "abcdefghij";
  let steamEngine = Engines.get("steam");
  let stirlingEngine = Engines.get("stirling");
  let metaWBO = new ServerWBO("global", {syncID: Service.syncID,
                                         storageVersion: STORAGE_VERSION,
                                         engines: {}});
  let server = sync_httpd_setup({
    "/1.0/johndoe/storage/meta/global": metaWBO.handler(),
    "/1.0/johndoe/storage/steam": new ServerWBO("steam", {}).handler(),
    "/1.0/johndoe/storage/stirling": new ServerWBO("stirling", {}).handler()
  });
  do_test_pending();
  setUp();

  try {
    _("Enable engine locally. Doing it on one is enough.");
    steamEngine.enabled = true;

    _("Sync.");
    Weave.Service.login();
    Weave.Service.sync();

    _("Meta record now contains the new engines.");
    do_check_true(!!metaWBO.data.engines.steam);
    do_check_true(!!metaWBO.data.engines.stirling);

    _("Engines continue to be enabled.");
    do_check_true(steamEngine.enabled);
    do_check_true(stirlingEngine.enabled);
  } finally {
    server.stop(do_test_finished);
    Service.startOver();
  }
}

function test_dependentEnginesDisabledLocally() {
  _("Test: Two dependent engines are enabled on remote clients and disabled locally");
  Service.syncID = "abcdefghij";
  let steamEngine = Engines.get("steam");
  let stirlingEngine = Engines.get("stirling");
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
    "/1.0/johndoe/storage/meta/global":     metaWBO.handler(),
    "/1.0/johndoe/storage/steam":           steamCollection.handler(),
    "/1.0/johndoe/storage/stirling":        stirlingCollection.handler()
  });
  do_test_pending();
  setUp();

  try {
    _("Disable engines locally. Doing it on one is enough.");
    Service._ignorePrefObserver = true;
    steamEngine.enabled = true;
    do_check_true(stirlingEngine.enabled);
    Service._ignorePrefObserver = false;
    steamEngine.enabled = false;
    do_check_false(stirlingEngine.enabled);

    _("Sync.");
    Weave.Service.login();
    Weave.Service.sync();

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
    server.stop(do_test_finished);
    Service.startOver();
  }
}

function run_test() {
  if (DISABLE_TESTS_BUG_604565)
    return;

  test_newAccount();
  test_enabledLocally();
  test_disabledLocally();
  test_enabledRemotely();
  test_disabledRemotely();
  test_disabledRemotelyTwoClients();
  test_dependentEnginesEnabledLocally();
  test_dependentEnginesDisabledLocally();
}
