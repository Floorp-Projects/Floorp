Cu.import("resource://weave/util.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/ext/Observers.js");


function SteamStore() {
  Store.call(this, "Steam");
  this.wasWiped = false;
}
SteamStore.prototype = {
  __proto__: Store.prototype,

  wipe: function() {
    this.wasWiped = true;
  }
};

function SteamTracker() {
  Tracker.call(this, "Steam");
}
SteamTracker.prototype = {
  __proto__: Tracker.prototype
};

function SteamEngine() {
  Engine.call(this, "Steam");
  this.wasReset = false;
  this.wasSynced = false;
}
SteamEngine.prototype = {
  __proto__: Engine.prototype,
  _storeObj: SteamStore,
  _trackerObj: SteamTracker,

  _resetClient: function () {
    this.wasReset = true;
  },

  _sync: function () {
    this.wasSynced = true;
  }
};

let engineObserver = {
  topics: [],

  observe: function(subject, topic, data) {
    do_check_eq(subject, "steam");
    this.topics.push(topic);
  },

  reset: function() {
    this.topics = [];
  }
};
Observers.add("weave:engine:reset-client:start", engineObserver);
Observers.add("weave:engine:reset-client:finish", engineObserver);
Observers.add("weave:engine:wipe-client:start", engineObserver);
Observers.add("weave:engine:wipe-client:finish", engineObserver);
Observers.add("weave:engine:sync:start", engineObserver);
Observers.add("weave:engine:sync:finish", engineObserver);


function do_check_throws(func) {
  var raised = false;
  try {
    func();
  } catch (ex) {
    raised = true;
  }
  do_check_true(raised);
}


function test_members() {
  _("Engine object members");
  let engine = new SteamEngine();
  do_check_eq(engine.displayName, "Steam");
  do_check_eq(engine.prefName, "steam");
  do_check_true(engine._store instanceof SteamStore);
  do_check_true(engine._tracker instanceof SteamTracker);
}

function test_score() {
  _("Engine.score corresponds to tracker.score and is readonly");
  let engine = new SteamEngine();
  do_check_eq(engine.score, 0);
  engine._tracker.score += 5;
  do_check_eq(engine.score, 5);

  do_check_throws(function() {
      engine.score = 10;
  });
}

function test_resetClient() {
  _("Engine.resetClient calls _resetClient");
  let engine = new SteamEngine();
  do_check_false(engine.wasReset);

  engine.resetClient();
  do_check_true(engine.wasReset);
  do_check_eq(engineObserver.topics[0], "weave:engine:reset-client:start");
  do_check_eq(engineObserver.topics[1], "weave:engine:reset-client:finish");

  engine.wasReset = false;
  engineObserver.reset();
}

function test_wipeClient() {
  _("Engine.wipeClient calls resetClient, wipes store, clears changed IDs");
  let engine = new SteamEngine();
  do_check_false(engine.wasReset);
  do_check_false(engine._store.wasWiped);
  do_check_true(engine._tracker.addChangedID("a-changed-id"));
  do_check_true("a-changed-id" in engine._tracker.changedIDs);

  engine.wipeClient();
  do_check_true(engine.wasReset);
  do_check_true(engine._store.wasWiped);
  do_check_eq(JSON.stringify(engine._tracker.changedIDs), "{}");
  do_check_eq(engineObserver.topics[0], "weave:engine:wipe-client:start");
  do_check_eq(engineObserver.topics[1], "weave:engine:reset-client:start");
  do_check_eq(engineObserver.topics[2], "weave:engine:reset-client:finish");
  do_check_eq(engineObserver.topics[3], "weave:engine:wipe-client:finish");

  engine.wasReset = false;
  engine._store.wasWiped = false;
  engineObserver.reset();
}

function test_enabled() {
  _("Engine.enabled corresponds to preference");
  let engine = new SteamEngine();
  try {
    do_check_false(engine.enabled);
    Svc.Prefs.set("engine.steam", true);
    do_check_true(engine.enabled);

    engine.enabled = false;
    do_check_false(Svc.Prefs.get("engine.steam"));
  } finally {
    Svc.Prefs.reset("engine.steam");
  }
}

function test_sync() {
  let engine = new SteamEngine();
  try {
    _("Engine.sync doesn't call _sync if it's not enabled");
    do_check_false(engine.enabled);
    do_check_false(engine.wasSynced);
    engine.sync();
    do_check_false(engine.wasSynced);

    _("Engine.sync calls _sync if it's enabled");
    engine.enabled = true;
    engine.sync();
    do_check_true(engine.wasSynced);
    do_check_eq(engineObserver.topics[0], "weave:engine:sync:start");
    do_check_eq(engineObserver.topics[1], "weave:engine:sync:finish");
  } finally {
    Svc.Prefs.reset("engine.steam");
    engine.wasSynced = false;
    engineObserver.reset();
  }
}
