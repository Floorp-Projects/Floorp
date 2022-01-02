/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { Observers } = ChromeUtils.import(
  "resource://services-common/observers.js"
);
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

function SteamStore(engine) {
  Store.call(this, "Steam", engine);
  this.wasWiped = false;
}
SteamStore.prototype = {
  __proto__: Store.prototype,

  async wipe() {
    this.wasWiped = true;
  },
};

function SteamTracker(name, engine) {
  LegacyTracker.call(this, name || "Steam", engine);
}
SteamTracker.prototype = {
  __proto__: LegacyTracker.prototype,
};

function SteamEngine(name, service) {
  SyncEngine.call(this, name, service);
  this.wasReset = false;
  this.wasSynced = false;
}
SteamEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: SteamStore,
  _trackerObj: SteamTracker,

  async _resetClient() {
    this.wasReset = true;
  },

  async _sync() {
    this.wasSynced = true;
  },
};

var engineObserver = {
  topics: [],

  observe(subject, topic, data) {
    Assert.equal(data, "steam");
    this.topics.push(topic);
  },

  reset() {
    this.topics = [];
  },
};
Observers.add("weave:engine:reset-client:start", engineObserver);
Observers.add("weave:engine:reset-client:finish", engineObserver);
Observers.add("weave:engine:wipe-client:start", engineObserver);
Observers.add("weave:engine:wipe-client:finish", engineObserver);
Observers.add("weave:engine:sync:start", engineObserver);
Observers.add("weave:engine:sync:finish", engineObserver);

async function cleanup(engine) {
  Svc.Prefs.resetBranch("");
  engine.wasReset = false;
  engine.wasSynced = false;
  engineObserver.reset();
  await engine._tracker.clearChangedIDs();
  await engine.finalize();
}

add_task(async function test_members() {
  _("Engine object members");
  let engine = new SteamEngine("Steam", Service);
  await engine.initialize();
  Assert.equal(engine.Name, "Steam");
  Assert.equal(engine.prefName, "steam");
  Assert.ok(engine._store instanceof SteamStore);
  Assert.ok(engine._tracker instanceof SteamTracker);
});

add_task(async function test_score() {
  _("Engine.score corresponds to tracker.score and is readonly");
  let engine = new SteamEngine("Steam", Service);
  await engine.initialize();
  Assert.equal(engine.score, 0);
  engine._tracker.score += 5;
  Assert.equal(engine.score, 5);

  try {
    engine.score = 10;
  } catch (ex) {
    // Setting an attribute that has a getter produces an error in
    // Firefox <= 3.6 and is ignored in later versions.  Either way,
    // the attribute's value won't change.
  }
  Assert.equal(engine.score, 5);
});

add_task(async function test_resetClient() {
  _("Engine.resetClient calls _resetClient");
  let engine = new SteamEngine("Steam", Service);
  await engine.initialize();
  Assert.ok(!engine.wasReset);

  await engine.resetClient();
  Assert.ok(engine.wasReset);
  Assert.equal(engineObserver.topics[0], "weave:engine:reset-client:start");
  Assert.equal(engineObserver.topics[1], "weave:engine:reset-client:finish");

  await cleanup(engine);
});

add_task(async function test_invalidChangedIDs() {
  _("Test that invalid changed IDs on disk don't end up live.");
  let engine = new SteamEngine("Steam", Service);
  await engine.initialize();
  let tracker = engine._tracker;

  await tracker._beforeSave();
  await OS.File.writeAtomic(
    tracker._storage.path,
    new TextEncoder().encode("5"),
    { tmpPath: tracker._storage.path + ".tmp" }
  );

  ok(!tracker._storage.dataReady);
  const changes = await tracker.getChangedIDs();
  changes.placeholder = true;
  deepEqual(
    changes,
    { placeholder: true },
    "Accessing changed IDs should load changes from disk as a side effect"
  );
  ok(tracker._storage.dataReady);

  Assert.ok(changes.placeholder);
  await cleanup(engine);
});

add_task(async function test_wipeClient() {
  _("Engine.wipeClient calls resetClient, wipes store, clears changed IDs");
  let engine = new SteamEngine("Steam", Service);
  await engine.initialize();
  Assert.ok(!engine.wasReset);
  Assert.ok(!engine._store.wasWiped);
  Assert.ok(await engine._tracker.addChangedID("a-changed-id"));
  let changes = await engine._tracker.getChangedIDs();
  Assert.ok("a-changed-id" in changes);

  await engine.wipeClient();
  Assert.ok(engine.wasReset);
  Assert.ok(engine._store.wasWiped);
  changes = await engine._tracker.getChangedIDs();
  Assert.equal(JSON.stringify(changes), "{}");
  Assert.equal(engineObserver.topics[0], "weave:engine:wipe-client:start");
  Assert.equal(engineObserver.topics[1], "weave:engine:reset-client:start");
  Assert.equal(engineObserver.topics[2], "weave:engine:reset-client:finish");
  Assert.equal(engineObserver.topics[3], "weave:engine:wipe-client:finish");

  await cleanup(engine);
});

add_task(async function test_enabled() {
  _("Engine.enabled corresponds to preference");
  let engine = new SteamEngine("Steam", Service);
  await engine.initialize();
  try {
    Assert.ok(!engine.enabled);
    Svc.Prefs.set("engine.steam", true);
    Assert.ok(engine.enabled);

    engine.enabled = false;
    Assert.ok(!Svc.Prefs.get("engine.steam"));
  } finally {
    await cleanup(engine);
  }
});

add_task(async function test_sync() {
  let engine = new SteamEngine("Steam", Service);
  await engine.initialize();
  try {
    _("Engine.sync doesn't call _sync if it's not enabled");
    Assert.ok(!engine.enabled);
    Assert.ok(!engine.wasSynced);
    await engine.sync();

    Assert.ok(!engine.wasSynced);

    _("Engine.sync calls _sync if it's enabled");
    engine.enabled = true;

    await engine.sync();
    Assert.ok(engine.wasSynced);
    Assert.equal(engineObserver.topics[0], "weave:engine:sync:start");
    Assert.equal(engineObserver.topics[1], "weave:engine:sync:finish");
  } finally {
    await cleanup(engine);
  }
});

add_task(async function test_disabled_no_track() {
  _("When an engine is disabled, its tracker is not tracking.");
  let engine = new SteamEngine("Steam", Service);
  await engine.initialize();
  let tracker = engine._tracker;
  Assert.equal(engine, tracker.engine);

  Assert.ok(!engine.enabled);
  Assert.ok(!tracker._isTracking);
  let changes = await tracker.getChangedIDs();
  do_check_empty(changes);

  Assert.ok(!tracker.engineIsEnabled());
  Assert.ok(!tracker._isTracking);
  changes = await tracker.getChangedIDs();
  do_check_empty(changes);

  let promisePrefChangeHandled = PromiseUtils.defer();
  const origMethod = tracker.onEngineEnabledChanged;
  tracker.onEngineEnabledChanged = async (...args) => {
    await origMethod.apply(tracker, args);
    promisePrefChangeHandled.resolve();
  };

  engine.enabled = true; // Also enables the tracker automatically.
  await promisePrefChangeHandled.promise;
  Assert.ok(tracker._isTracking);
  changes = await tracker.getChangedIDs();
  do_check_empty(changes);

  await tracker.addChangedID("abcdefghijkl");
  changes = await tracker.getChangedIDs();
  Assert.ok(0 < changes.abcdefghijkl);
  promisePrefChangeHandled = PromiseUtils.defer();
  Svc.Prefs.set("engine." + engine.prefName, false);
  await promisePrefChangeHandled.promise;
  Assert.ok(!tracker._isTracking);
  changes = await tracker.getChangedIDs();
  do_check_empty(changes);

  await cleanup(engine);
});
