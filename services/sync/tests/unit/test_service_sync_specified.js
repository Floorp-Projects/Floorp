/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

initTestLogging();
Service.engineManager.clear();

let syncedEngines = []

function SteamEngine() {
  SyncEngine.call(this, "Steam", Service);
}
SteamEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _sync: function _sync() {
    syncedEngines.push(this.name);
  }
};
Service.engineManager.register(SteamEngine);

function StirlingEngine() {
  SyncEngine.call(this, "Stirling", Service);
}
StirlingEngine.prototype = {
  __proto__: SteamEngine.prototype,
  _sync: function _sync() {
    syncedEngines.push(this.name);
  }
};
Service.engineManager.register(StirlingEngine);

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

function setUp() {
  syncedEngines = [];
  let engine = Service.engineManager.get("steam");
  engine.enabled = true;
  engine.syncPriority = 1;

  engine = Service.engineManager.get("stirling");
  engine.enabled = true;
  engine.syncPriority = 2;

  let server = sync_httpd_setup({
    "/1.1/johndoe/storage/meta/global": new ServerWBO("global", {}).handler(),
  });
  new SyncTestingInfrastructure(server, "johndoe", "ilovejane",
                                "abcdeabcdeabcdeabcdeabcdea");
  return server;
}

function run_test() {
  initTestLogging("Trace");
  validate_all_future_pings();
  Log.repository.getLogger("Sync.Service").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.ErrorHandler").level = Log.Level.Trace;

  run_next_test();
}

add_test(function test_noEngines() {
  _("Test: An empty array of engines to sync does nothing.");
  let server = setUp();

  try {
    _("Sync with no engines specified.");
    Service.sync([]);
    deepEqual(syncedEngines, [], "no engines were synced");

  } finally {
    Service.startOver();
    server.stop(run_next_test);
  }
});

add_test(function test_oneEngine() {
  _("Test: Only one engine is synced.");
  let server = setUp();

  try {

    _("Sync with 1 engine specified.");
    Service.sync(["steam"]);
    deepEqual(syncedEngines, ["steam"])

  } finally {
    Service.startOver();
    server.stop(run_next_test);
  }
});

add_test(function test_bothEnginesSpecified() {
  _("Test: All engines are synced when specified in the correct order (1).");
  let server = setUp();

  try {
    _("Sync with both engines specified.");
    Service.sync(["steam", "stirling"]);
    deepEqual(syncedEngines, ["steam", "stirling"])

  } finally {
    Service.startOver();
    server.stop(run_next_test);
  }
});

add_test(function test_bothEnginesSpecified() {
  _("Test: All engines are synced when specified in the correct order (2).");
  let server = setUp();

  try {
    _("Sync with both engines specified.");
    Service.sync(["stirling", "steam"]);
    deepEqual(syncedEngines, ["stirling", "steam"])

  } finally {
    Service.startOver();
    server.stop(run_next_test);
  }
});

add_test(function test_bothEnginesDefault() {
  _("Test: All engines are synced when nothing is specified.");
  let server = setUp();

  try {
    Service.sync();
    deepEqual(syncedEngines, ["steam", "stirling"])

  } finally {
    Service.startOver();
    server.stop(run_next_test);
  }
});
