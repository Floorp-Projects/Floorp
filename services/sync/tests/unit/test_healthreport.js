/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Metrics.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://services-sync/main.js", this);
Cu.import("resource://services-sync/healthreport.jsm", this);
Cu.import("resource://testing-common/services-common/logging.js", this);
Cu.import("resource://testing-common/services/healthreport/utils.jsm", this);

function run_test() {
  initTestLogging();

  run_next_test();
}

add_task(function test_constructor() {
  let provider = new SyncProvider();
});

// Provider can initialize and de-initialize properly.
add_task(function* test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new SyncProvider();
  yield provider.init(storage);
  yield provider.shutdown();
  yield storage.close();
});

add_task(function* test_collect() {
  let storage = yield Metrics.Storage("collect");
  let provider = new SyncProvider();
  yield provider.init(storage);

  // Initially nothing should be configured.
  let now = new Date();
  yield provider.collectDailyData();

  let m = provider.getMeasurement("sync", 1);
  let values = yield m.getValues();
  Assert.equal(values.days.size, 1);
  Assert.ok(values.days.hasDay(now));
  let day = values.days.getDay(now);
  Assert.ok(day.has("enabled"));
  Assert.ok(day.has("activeProtocol"));
  Assert.ok(day.has("preferredProtocol"));
  Assert.equal(day.get("enabled"), 0);
  Assert.equal(day.get("preferredProtocol"), "1.5");
  Assert.equal(day.get("activeProtocol"), "1.5",
               "Protocol without setup should be FX Accounts version.");

  // Now check for old Sync setup.
  let branch = new Preferences("services.sync.");
  branch.set("username", "foo");
  branch.reset("fxaccounts.enabled");
  yield provider.collectDailyData();
  values = yield m.getValues();
  Assert.equal(values.days.getDay(now).get("activeProtocol"), "1.1",
               "Protocol with old Sync setup is correct.");

  Assert.equal(Weave.Status.__authManager, undefined, "Detect code changes");

  // Let's enable Sync so we can get more useful data.
  // We need to do this because the FHR probe only records more info if Sync
  // is configured properly.
  Weave.Service.identity.account = "johndoe";
  Weave.Service.identity.basicPassword = "ilovejane";
  Weave.Service.identity.syncKey = Weave.Utils.generatePassphrase();
  Weave.Service.clusterURL = "http://localhost/";
  Assert.equal(Weave.Status.checkSetup(), Weave.STATUS_OK);

  yield provider.collectDailyData();
  values = yield m.getValues();
  day = values.days.getDay(now);
  Assert.equal(day.get("enabled"), 1);

  // An empty account should have 1 device: us.
  let dm = provider.getMeasurement("devices", 1);
  values = yield dm.getValues();
  Assert.ok(values.days.hasDay(now));
  day = values.days.getDay(now);
  Assert.equal(day.size, 1);
  let engine = Weave.Service.clientsEngine;
  Assert.ok(engine);
  Assert.ok(day.has(engine.localType));
  Assert.equal(day.get(engine.localType), 1);

  // Add some devices and ensure they show up.
  engine._store._remoteClients["id1"] = {type: "mobile"};
  engine._store._remoteClients["id2"] = {type: "tablet"};
  engine._store._remoteClients["id3"] = {type: "mobile"};

  yield provider.collectDailyData();
  values = yield dm.getValues();
  day = values.days.getDay(now);

  let expected = {
    "foobar": 0,
    "tablet": 1,
    "mobile": 2,
    "desktop": 0,
  };

  for (let type in expected) {
    let count = expected[type];

    if (engine.localType == type) {
      count++;
    }

    if (!count) {
      Assert.ok(!day.has(type));
    } else {
      Assert.ok(day.has(type));
      Assert.equal(day.get(type), count);
    }
  }

  engine._store._remoteClients = {};

  yield provider.shutdown();
  yield storage.close();
});

add_task(function* test_sync_events() {
  let storage = yield Metrics.Storage("sync_events");
  let provider = new SyncProvider();
  yield provider.init(storage);

  let m = provider.getMeasurement("sync", 1);

  for (let i = 0; i < 5; i++) {
    Services.obs.notifyObservers(null, "weave:service:sync:start", null);
  }

  for (let i = 0; i < 3; i++) {
    Services.obs.notifyObservers(null, "weave:service:sync:finish", null);
  }

  for (let i = 0; i < 2; i++) {
    Services.obs.notifyObservers(null, "weave:service:sync:error", null);
  }

  // Wait for storage to complete.
  yield m.storage.enqueueOperation(() => {
    return Promise.resolve();
  });

  let values = yield m.getValues();
  let now = new Date();
  Assert.ok(values.days.hasDay(now));
  let day = values.days.getDay(now);

  Assert.ok(day.has("syncStart"));
  Assert.ok(day.has("syncSuccess"));
  Assert.ok(day.has("syncError"));
  Assert.equal(day.get("syncStart"), 5);
  Assert.equal(day.get("syncSuccess"), 3);
  Assert.equal(day.get("syncError"), 2);

  yield provider.shutdown();
  yield storage.close();
});

add_task(function* test_healthreporter_json() {
  let reporter = yield getHealthReporter("healthreporter_json");
  yield reporter.init();
  try {
    yield reporter._providerManager.registerProvider(new SyncProvider());
    yield reporter.collectMeasurements();
    let payload = yield reporter.getJSONPayload(true);
    let now = new Date();
    let today = reporter._formatDate(now);

    Assert.ok(today in payload.data.days);
    let day = payload.data.days[today];

    Assert.ok("org.mozilla.sync.sync" in day);
    Assert.ok("org.mozilla.sync.devices" in day);

    let devices = day["org.mozilla.sync.devices"];
    let engine = Weave.Service.clientsEngine;
    Assert.ok(engine);
    let type = engine.localType;
    Assert.ok(type);
    Assert.ok(type in devices);
    Assert.equal(devices[type], 1);
  } finally {
    reporter._shutdown();
  }
});
