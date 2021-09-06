"use strict";

const { ExperimentStore } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentStore.jsm"
);
const { FeatureManifest } = ChromeUtils.import(
  "resource://nimbus/FeatureManifest.js"
);

const { SYNC_DATA_PREF_BRANCH, SYNC_DEFAULTS_PREF_BRANCH } = ExperimentStore;
const { cleanupStorePrefCache } = ExperimentFakes;

add_task(async function test_sharedDataMap_key() {
  const store = new ExperimentStore();

  // Outside of tests we use sharedDataKey for the profile dir filepath
  // where we store experiments
  Assert.ok(store._sharedDataKey, "Make sure it's defined");
});

add_task(async function test_usageBeforeInitialization() {
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "purple", enabled: true },
    },
  });

  Assert.equal(store.getAll().length, 0, "It should not fail");

  await store.init();
  store.addExperiment(experiment);

  Assert.equal(
    store.getExperimentForFeature("purple"),
    experiment,
    "should return a matching experiment for the given feature"
  );
});

add_task(async function test_event_add_experiment() {
  const sandbox = sinon.createSandbox();
  const store = ExperimentFakes.store();
  const expected = ExperimentFakes.experiment("foo");
  const updateEventCbStub = sandbox.stub();

  // Setup ExperimentManager and child store for ExperimentAPI
  await store.init();

  // Set update cb
  store.on("update:foo", updateEventCbStub);

  // Add some data
  store.addExperiment(expected);

  Assert.equal(updateEventCbStub.callCount, 1, "Called once for add");

  store.off("update:foo", updateEventCbStub);
});

add_task(async function test_event_updates_main() {
  const sandbox = sinon.createSandbox();
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo");
  const updateEventCbStub = sandbox.stub();

  // Setup ExperimentManager and child store for ExperimentAPI
  await store.init();

  // Set update cb
  store.on(`update:${experiment.branch.feature.featureId}`, updateEventCbStub);

  store.addExperiment(experiment);
  store.updateExperiment("foo", { active: false });

  Assert.equal(
    updateEventCbStub.callCount,
    2,
    "Should be called twice: add, update"
  );
  Assert.equal(
    updateEventCbStub.secondCall.args[1].active,
    false,
    "Should be called with updated experiment status"
  );

  store.off(`update:${experiment.branch.feature.featureId}`, updateEventCbStub);
});

add_task(async function test_getExperimentForGroup() {
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "purple", enabled: true },
    },
  });

  await store.init();
  store.addExperiment(ExperimentFakes.experiment("bar"));
  store.addExperiment(experiment);

  Assert.equal(
    store.getExperimentForFeature("purple"),
    experiment,
    "should return a matching experiment for the given feature"
  );
});

add_task(async function test_hasExperimentForFeature() {
  const store = ExperimentFakes.store();

  await store.init();
  store.addExperiment(
    ExperimentFakes.experiment("foo", {
      branch: {
        slug: "variant",
        feature: { featureId: "green", enabled: true },
      },
    })
  );
  store.addExperiment(
    ExperimentFakes.experiment("foo2", {
      branch: {
        slug: "variant",
        feature: { featureId: "yellow", enabled: true },
      },
    })
  );
  store.addExperiment(
    ExperimentFakes.experiment("bar_expired", {
      active: false,
      branch: {
        slug: "variant",
        feature: { featureId: "purple", enabled: true },
      },
    })
  );
  Assert.equal(
    store.hasExperimentForFeature(),
    false,
    "should return false if the input is empty"
  );

  Assert.equal(
    store.hasExperimentForFeature(undefined),
    false,
    "should return false if the input is undefined"
  );

  Assert.equal(
    store.hasExperimentForFeature("green"),
    true,
    "should return true if there is an experiment with any of the given groups"
  );

  Assert.equal(
    store.hasExperimentForFeature("purple"),
    false,
    "should return false if there is a non-active experiment with the given groups"
  );
});

add_task(async function test_getAll_getAllActive() {
  const store = ExperimentFakes.store();

  await store.init();
  ["foo", "bar", "baz"].forEach(slug =>
    store.addExperiment(ExperimentFakes.experiment(slug, { active: false }))
  );
  store.addExperiment(ExperimentFakes.experiment("qux", { active: true }));

  Assert.deepEqual(
    store.getAll().map(e => e.slug),
    ["foo", "bar", "baz", "qux"],
    ".getAll() should return all experiments"
  );
  Assert.deepEqual(
    store.getAllActive().map(e => e.slug),
    ["qux"],
    ".getAllActive() should return all experiments that are active"
  );
});

add_task(async function test_addExperiment() {
  const store = ExperimentFakes.store();
  const exp = ExperimentFakes.experiment("foo");

  await store.init();
  store.addExperiment(exp);

  Assert.equal(store.get("foo"), exp, "should save experiment by slug");
});

add_task(async function test_updateExperiment() {
  const feature = { featureId: "cfr", enabled: true };
  const experiment = Object.freeze(
    ExperimentFakes.experiment("foo", { feature, active: true })
  );
  const store = ExperimentFakes.store();

  await store.init();
  store.addExperiment(experiment);
  store.updateExperiment("foo", { active: false });

  const actual = store.get("foo");
  Assert.equal(actual.active, false, "should change updated props");
  Assert.deepEqual(
    actual.branch.feature,
    feature,
    "should not update other props"
  );
});

add_task(async function test_sync_access_before_init() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();

  Assert.equal(store.getAll().length, 0, "Start with an empty store");

  const syncAccessExp = ExperimentFakes.experiment("foo", {
    feature: { featureId: "newtab", enabled: "true" },
  });
  await store.init();
  store.addExperiment(syncAccessExp);

  let prefValue;
  try {
    prefValue = JSON.parse(
      Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}newtab`)
    );
  } catch (e) {
    Assert.ok(false, "Failed to parse pref value");
  }

  Assert.ok(prefValue, "Parsed stored experiment");
  Assert.equal(prefValue.slug, syncAccessExp.slug, "Got back the experiment");

  // New un-initialized store that should read the pref value
  store = ExperimentFakes.store();

  Assert.equal(
    store.getExperimentForFeature("newtab").slug,
    "foo",
    "Returns experiment from pref"
  );
});

add_task(async function test_sync_access_update() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();
  let experiment = ExperimentFakes.experiment("foo", {
    feature: { featureId: "aboutwelcome", enabled: true },
  });

  await store.init();

  store.addExperiment(experiment);
  store.updateExperiment("foo", {
    branch: {
      ...experiment.branch,
      feature: {
        featureId: "aboutwelcome",
        enabled: true,
        value: { bar: "bar" },
      },
    },
  });

  store = ExperimentFakes.store();
  let cachedExperiment = store.getExperimentForFeature("aboutwelcome");

  Assert.ok(cachedExperiment, "Got back 1 experiment");
  Assert.deepEqual(
    cachedExperiment.branch.feature.value,
    { bar: "bar" },
    "Got updated value"
  );
});

add_task(async function test_sync_features_only() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();
  let experiment = ExperimentFakes.experiment("foo", {
    feature: { featureId: "cfr", enabled: true },
  });

  await store.init();

  store.addExperiment(experiment);
  store = ExperimentFakes.store();

  Assert.equal(store.getAll().length, 0, "cfr is not a sync access experiment");
});

add_task(async function test_sync_features_remotely() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();
  let experiment = ExperimentFakes.experiment("foo", {
    feature: { featureId: "cfr", enabled: true, isEarlyStartup: true },
  });

  await store.init();

  store.addExperiment(experiment);
  store = ExperimentFakes.store();

  Assert.ok(
    Services.prefs.prefHasUserValue("nimbus.syncdatastore.cfr"),
    "The cfr feature was stored as early access in prefs"
  );
  Assert.equal(store.getAll().length, 0, "Featre restored from prefs");
});

add_task(async function test_sync_access_unenroll() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();
  let experiment = ExperimentFakes.experiment("foo", {
    feature: { featureId: "aboutwelcome", enabled: true },
    active: true,
  });

  await store.init();

  store.addExperiment(experiment);
  store.updateExperiment("foo", { active: false });

  store = ExperimentFakes.store();
  let experiments = store.getAll();

  Assert.equal(experiments.length, 0, "Unenrolled experiment is deleted");
});

add_task(async function test_sync_access_unenroll_2() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();
  let experiment1 = ExperimentFakes.experiment("foo", {
    feature: { featureId: "newtab", enabled: true },
  });
  let experiment2 = ExperimentFakes.experiment("bar", {
    feature: { featureId: "aboutwelcome", enabled: true },
  });

  await store.init();

  store.addExperiment(experiment1);
  store.addExperiment(experiment2);

  Assert.equal(store.getAll().length, 2, "2/2 experiments");

  let other_store = ExperimentFakes.store();

  Assert.ok(
    other_store.getExperimentForFeature("aboutwelcome"),
    "Fetches experiment from pref cache even before init (aboutwelcome)"
  );

  store.updateExperiment("bar", { active: false });

  Assert.ok(
    other_store.getExperimentForFeature("newtab").slug,
    "Fetches experiment from pref cache even before init (newtab)"
  );
  Assert.ok(
    !other_store.getExperimentForFeature("aboutwelcome")?.slug,
    "Experiment was updated and should not be found"
  );

  store.updateExperiment("foo", { active: false });
  Assert.ok(
    !other_store.getExperimentForFeature("newtab")?.slug,
    "Unenrolled from 2/2 experiments"
  );

  Assert.equal(
    Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}newtab`, "").length,
    0,
    "Cleared pref 1"
  );
  Assert.equal(
    Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}aboutwelcome`, "")
      .length,
    0,
    "Cleared pref 2"
  );
});

add_task(async function test_syncDataStore_setDefault() {
  cleanupStorePrefCache();
  const store = ExperimentFakes.store();

  await store.init();

  Assert.equal(
    Services.prefs.getStringPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`,
      ""
    ),
    "",
    "Pref is empty"
  );

  store.updateRemoteConfigs("aboutwelcome", { remote: true });

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Stored in pref"
  );

  cleanupStorePrefCache();
});

add_task(async function test_syncDataStore_getDefault() {
  cleanupStorePrefCache();
  const store = ExperimentFakes.store();

  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`,
    JSON.stringify({ remote: true })
  );

  let data = store.getRemoteConfig("aboutwelcome");

  Assert.ok(data.remote, "Restore data from pref");

  cleanupStorePrefCache();
});

add_task(async function test_updateRemoteConfigs() {
  const sandbox = sinon.createSandbox();
  const store = ExperimentFakes.store();
  const stub = sandbox.stub();
  const value = { bar: true };

  store._onFeatureUpdate("featureId", stub);

  await store.init();
  store.updateRemoteConfigs("featureId", value);

  Assert.deepEqual(
    store.getRemoteConfig("featureId"),
    value,
    "should return the stored value"
  );
  Assert.equal(stub.callCount, 1, "Called once on update");
  Assert.equal(
    stub.firstCall.args[1],
    "remote-defaults-update",
    "Called for correct reason"
  );
});

add_task(async function test_finalizaRemoteConfigs_cleanup() {
  cleanupStorePrefCache();
  const store = ExperimentFakes.store();

  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}unit-test-feature`,
    JSON.stringify({ remote: true })
  );

  // We are able to sync-read data without needing to initialize the store
  let data = store.getRemoteConfig("unit-test-feature");
  Assert.ok(data.remote, "Restore data from pref");

  // We need to initialize the store for the cleanup step
  await store.init();

  store.finalizeRemoteConfigs([]);
  data = store.getRemoteConfig("unit-test-feature");

  Assert.ok(!data, `Data was removed ${JSON.stringify(data)}`);

  cleanupStorePrefCache();
});

add_task(async function test_finalizaRemoteConfigs_cleanup() {
  cleanupStorePrefCache();
  const store = ExperimentFakes.store();
  await store.init();

  store.updateRemoteConfigs("aboutwelcome", { remote: true });

  let data = store.getRemoteConfig("aboutwelcome");
  Assert.ok(data.remote, "Restore data from pref");

  store.finalizeRemoteConfigs(["aboutwelcome"]);
  data = store.getRemoteConfig("aboutwelcome");

  Assert.ok(data.remote, "Data was kept");

  cleanupStorePrefCache();
});

add_task(async function test_getAllExistingRemoteConfigIds() {
  cleanupStorePrefCache();
  const store = ExperimentFakes.store();

  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}unit-test-feature`,
    JSON.stringify({ remote: true })
  );

  await store.init();

  store.updateRemoteConfigs("aboutwelcome", { remote: true });
  store.updateRemoteConfigs("unit-test-feature", { remote: true });

  let data = store.getAllExistingRemoteConfigIds();

  Assert.deepEqual(
    data,
    ["aboutwelcome", "unit-test-feature"],
    "Should return ids from sync pref cache and in memory store without duplication"
  );

  cleanupStorePrefCache();
});

add_task(async function test_storeValuePerPref_noVariables() {
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: {
        // Ensure it gets saved to prefs
        isEarlyStartup: true,
        featureId: "purple",
        enabled: true,
      },
    },
  });

  await store.init();
  store.addExperiment(experiment);

  let branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`, ""),
    "Experiment metadata saved to prefs"
  );

  Assert.equal(branch.getChildList("").length, 0, "No variables to store");

  store._updateSyncStore({ ...experiment, active: false });
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`, ""),
    "Experiment cleanup"
  );
});

add_task(async function test_storeValuePerPref_withVariables() {
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: {
        // Ensure it gets saved to prefs
        isEarlyStartup: true,
        featureId: "purple",
        value: { color: "purple", enabled: true },
      },
    },
  });

  await store.init();
  store.addExperiment(experiment);

  let branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  Assert.equal(
    Services.prefs
      .getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`)
      .indexOf("color"),
    -1,
    "Experiment metadata does not contain variables"
  );

  Assert.equal(branch.getChildList("").length, 2, "Enabled and color");

  store._updateSyncStore({ ...experiment, active: false });
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`, ""),
    "Experiment cleanup"
  );
  Assert.equal(branch.getChildList("").length, 0, "Variables are also removed");
});

add_task(async function test_storeValuePerPref_returnsSameValue() {
  let store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: {
        // Ensure it gets saved to prefs
        isEarlyStartup: true,
        featureId: "purple",
        value: { color: "purple", enabled: true },
      },
    },
  });

  await store.init();
  store.addExperiment(experiment);
  let branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  store = ExperimentFakes.store();
  Assert.deepEqual(
    store.getExperimentForFeature("purple"),
    experiment,
    "Returns the same value"
  );

  // Cleanup
  store._updateSyncStore({ ...experiment, active: false });
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`, ""),
    "Experiment cleanup"
  );
  Assert.deepEqual(branch.getChildList(""), [], "Variables are also removed");
});

add_task(async function test_storeValuePerPref_returnsSameValue_allTypes() {
  let store = ExperimentFakes.store();
  // Add a fake feature that matches the variables we're testing
  FeatureManifest.purple = {
    variables: {
      string: { type: "string" },
      bool: { type: "boolean" },
      array: { type: "json" },
      number1: { type: "int" },
      number2: { type: "int" },
      number3: { type: "int" },
      json: { type: "json" },
    },
  };
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: {
        // Ensure it gets saved to prefs
        isEarlyStartup: true,
        featureId: "purple",
        value: {
          string: "string",
          bool: true,
          array: [1, 2, 3],
          number1: 42,
          number2: 0,
          number3: -5,
          json: { jsonValue: true },
        },
      },
    },
  });

  await store.init();
  store.addExperiment(experiment);
  let branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  store = ExperimentFakes.store();
  Assert.deepEqual(
    store.getExperimentForFeature("purple").branch.feature.value,
    experiment.branch.feature.value,
    "Returns the same value"
  );

  // Cleanup
  store._updateSyncStore({ ...experiment, active: false });
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`, ""),
    "Experiment cleanup"
  );
  Assert.deepEqual(branch.getChildList(""), [], "Variables are also removed");
  delete FeatureManifest.purple;
});

add_task(async function test_cleanupOldRecipes() {
  let store = ExperimentFakes.store();
  let sandbox = sinon.createSandbox();
  let stub = sandbox.stub(store, "_removeEntriesByKeys");
  const experiment1 = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "purple", enabled: true },
    },
  });
  const experiment2 = ExperimentFakes.experiment("bar", {
    branch: {
      slug: "variant",
      feature: { featureId: "purple", enabled: true },
    },
  });
  const experiment3 = ExperimentFakes.experiment("baz", {
    branch: {
      slug: "variant",
      feature: { featureId: "purple", enabled: true },
    },
  });
  const experiment4 = ExperimentFakes.experiment("faz", {
    branch: {
      slug: "variant",
      feature: { featureId: "purple", enabled: true },
    },
  });
  // Exp 2 is kept because it's recent (even though it's not active)
  // Exp 4 is kept because it's active
  experiment2.lastSeen = new Date().toISOString();
  experiment2.active = false;
  experiment1.lastSeen = new Date("2020-01-01").toISOString();
  experiment1.active = false;
  experiment3.active = false;
  delete experiment3.lastSeen;
  store._data = {
    foo: experiment1,
    bar: experiment2,
    baz: experiment3,
    faz: experiment4,
  };

  store._cleanupOldRecipes();

  Assert.ok(stub.calledOnce, "Recipe cleanup called");
  Assert.equal(
    stub.firstCall.args[0].length,
    2,
    "We call to remove enrollments"
  );
  Assert.equal(
    stub.firstCall.args[0][0],
    experiment1.slug,
    "Should remove expired enrollment"
  );
  Assert.equal(
    stub.firstCall.args[0][1],
    experiment3.slug,
    "Should remove invalid enrollment"
  );
});
