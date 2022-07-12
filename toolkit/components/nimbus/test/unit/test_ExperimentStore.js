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
      features: [{ featureId: "purple" }],
    },
  });

  Assert.equal(store.getAll().length, 0, "It should not fail");

  await store.init();
  store.addEnrollment(experiment);

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
  store.addEnrollment(expected);

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
  store.on(
    `update:${experiment.branch.features[0].featureId}`,
    updateEventCbStub
  );

  store.addEnrollment(experiment);
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

  store.off(
    `update:${experiment.branch.features[0].featureId}`,
    updateEventCbStub
  );
});

add_task(async function test_getExperimentForGroup() {
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [{ featureId: "purple" }],
    },
  });

  await store.init();
  store.addEnrollment(ExperimentFakes.experiment("bar"));
  store.addEnrollment(experiment);

  Assert.equal(
    store.getExperimentForFeature("purple"),
    experiment,
    "should return a matching experiment for the given feature"
  );
});

add_task(async function test_hasExperimentForFeature() {
  const store = ExperimentFakes.store();

  await store.init();
  store.addEnrollment(
    ExperimentFakes.experiment("foo", {
      branch: {
        slug: "variant",
        feature: { featureId: "green" },
      },
    })
  );
  store.addEnrollment(
    ExperimentFakes.experiment("foo2", {
      branch: {
        slug: "variant",
        feature: { featureId: "yellow" },
      },
    })
  );
  store.addEnrollment(
    ExperimentFakes.experiment("bar_expired", {
      active: false,
      branch: {
        slug: "variant",
        feature: { featureId: "purple" },
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
    store.addEnrollment(ExperimentFakes.experiment(slug, { active: false }))
  );
  store.addEnrollment(ExperimentFakes.experiment("qux", { active: true }));

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

add_task(async function test_getAll_getAllActive_no_rollouts() {
  const store = ExperimentFakes.store();

  await store.init();
  ["foo", "bar", "baz"].forEach(slug =>
    store.addEnrollment(ExperimentFakes.experiment(slug, { active: false }))
  );
  store.addEnrollment(ExperimentFakes.experiment("qux", { active: true }));
  store.addEnrollment(ExperimentFakes.rollout("rol"));

  Assert.deepEqual(
    store.getAll().map(e => e.slug),
    ["foo", "bar", "baz", "qux", "rol"],
    ".getAll() should return all experiments and rollouts"
  );
  Assert.deepEqual(
    store.getAllActive().map(e => e.slug),
    ["qux"],
    ".getAllActive() should return all experiments that are active and no rollouts"
  );
});

add_task(async function test_getAllRollouts() {
  const store = ExperimentFakes.store();

  await store.init();
  ["foo", "bar", "baz"].forEach(slug =>
    store.addEnrollment(ExperimentFakes.rollout(slug))
  );
  store.addEnrollment(ExperimentFakes.experiment("qux", { active: true }));

  Assert.deepEqual(
    store.getAll().map(e => e.slug),
    ["foo", "bar", "baz", "qux"],
    ".getAll() should return all experiments and rollouts"
  );
  Assert.deepEqual(
    store.getAllRollouts().map(e => e.slug),
    ["foo", "bar", "baz"],
    ".getAllRollouts() should return all rollouts"
  );
});

add_task(async function test_addEnrollment_experiment() {
  const store = ExperimentFakes.store();
  const exp = ExperimentFakes.experiment("foo");

  await store.init();
  store.addEnrollment(exp);

  Assert.equal(store.get("foo"), exp, "should save experiment by slug");
});

add_task(async function test_addEnrollment_rollout() {
  const store = ExperimentFakes.store();
  const rollout = ExperimentFakes.rollout("foo");

  await store.init();
  store.addEnrollment(rollout);

  Assert.equal(store.get("foo"), rollout, "should save rollout by slug");
});

add_task(async function test_updateExperiment() {
  const features = [{ featureId: "cfr" }];
  const experiment = Object.freeze(
    ExperimentFakes.experiment("foo", { features, active: true })
  );
  const store = ExperimentFakes.store();

  await store.init();
  store.addEnrollment(experiment);
  store.updateExperiment("foo", { active: false });

  const actual = store.get("foo");
  Assert.equal(actual.active, false, "should change updated props");
  Assert.deepEqual(
    actual.branch.features,
    features,
    "should not update other props"
  );
});

add_task(async function test_sync_access_before_init() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();

  Assert.equal(store.getAll().length, 0, "Start with an empty store");

  const syncAccessExp = ExperimentFakes.experiment("foo", {
    features: [{ featureId: "newtab" }],
  });
  await store.init();
  store.addEnrollment(syncAccessExp);

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
    features: [{ featureId: "aboutwelcome" }],
  });

  await store.init();

  store.addEnrollment(experiment);
  store.updateExperiment("foo", {
    branch: {
      ...experiment.branch,
      features: [
        {
          featureId: "aboutwelcome",
          value: { bar: "bar", enabled: true },
        },
      ],
    },
  });

  store = ExperimentFakes.store();
  let cachedExperiment = store.getExperimentForFeature("aboutwelcome");

  Assert.ok(cachedExperiment, "Got back 1 experiment");
  Assert.deepEqual(
    // `branch.feature` and not `features` because for sync access (early startup)
    // experiments we only store the `isEarlyStartup` feature
    cachedExperiment.branch.feature.value,
    { bar: "bar", enabled: true },
    "Got updated value"
  );
});

add_task(async function test_sync_features_only() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();
  let experiment = ExperimentFakes.experiment("foo", {
    features: [{ featureId: "cfr" }],
  });

  await store.init();

  store.addEnrollment(experiment);
  store = ExperimentFakes.store();

  Assert.equal(store.getAll().length, 0, "cfr is not a sync access experiment");
});

add_task(async function test_sync_features_remotely() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();
  let experiment = ExperimentFakes.experiment("foo", {
    features: [{ featureId: "cfr", isEarlyStartup: true }],
  });

  await store.init();

  store.addEnrollment(experiment);
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
    features: [{ featureId: "aboutwelcome" }],
    active: true,
  });

  await store.init();

  store.addEnrollment(experiment);
  store.updateExperiment("foo", { active: false });

  store = ExperimentFakes.store();
  let experiments = store.getAll();

  Assert.equal(experiments.length, 0, "Unenrolled experiment is deleted");
});

add_task(async function test_sync_access_unenroll_2() {
  cleanupStorePrefCache();

  let store = ExperimentFakes.store();
  let experiment1 = ExperimentFakes.experiment("foo", {
    features: [{ featureId: "newtab" }],
  });
  let experiment2 = ExperimentFakes.experiment("bar", {
    features: [{ featureId: "aboutwelcome" }],
  });

  await store.init();

  store.addEnrollment(experiment1);
  store.addEnrollment(experiment2);

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

add_task(async function test_getRolloutForFeature_fromStore() {
  const store = ExperimentFakes.store();
  const rollout = ExperimentFakes.rollout("foo");

  await store.init();
  store.addEnrollment(rollout);

  Assert.deepEqual(
    store.getRolloutForFeature(rollout.featureIds[0]),
    rollout,
    "Should return back the same rollout"
  );
});

add_task(async function test_getRolloutForFeature_fromSyncCache() {
  let store = ExperimentFakes.store();
  const rollout = ExperimentFakes.rollout("foo", {
    branch: {
      slug: "early-startup",
      features: [{ featureId: "aboutwelcome", value: { enabled: true } }],
    },
  });
  let updatePromise = new Promise(resolve =>
    store.on(`update:${rollout.slug}`, resolve)
  );

  await store.init();
  store.addEnrollment(rollout);
  await updatePromise;
  // New uninitialized store will return data from sync cache
  // before init
  store = ExperimentFakes.store();

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Sync cache is set"
  );
  Assert.equal(
    store.getRolloutForFeature(rollout.featureIds[0]).slug,
    rollout.slug,
    "Should return back the same rollout"
  );
  Assert.deepEqual(
    store.getRolloutForFeature(rollout.featureIds[0]).branch.feature,
    rollout.branch.features[0],
    "Should return back the same feature"
  );
  cleanupStorePrefCache();
});

add_task(async function test_remoteRollout() {
  let store = ExperimentFakes.store();
  const rollout = ExperimentFakes.rollout("foo", {
    branch: {
      slug: "early-startup",
      features: [{ featureId: "aboutwelcome", value: { enabled: true } }],
    },
  });
  let featureUpdateStub = sinon.stub();
  let updatePromise = new Promise(resolve =>
    store.on(`update:${rollout.slug}`, resolve)
  );
  store.on("update:aboutwelcome", featureUpdateStub);

  await store.init();
  store.addEnrollment(rollout);
  await updatePromise;

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Sync cache is set"
  );

  updatePromise = new Promise(resolve =>
    store.on(`update:${rollout.slug}`, resolve)
  );
  store.updateExperiment(rollout.slug, { active: false });

  // wait for it to be removed
  await updatePromise;

  Assert.ok(featureUpdateStub.calledTwice, "Called for add and remove");
  Assert.ok(
    store.get(rollout.slug),
    "Rollout is still in the store just not active"
  );
  Assert.ok(
    !store.getRolloutForFeature("aboutwelcome"),
    "Feature rollout should not exist"
  );
  Assert.ok(
    !Services.prefs.getStringPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`,
      ""
    ),
    "Sync cache is cleared"
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

  let rollout = ExperimentFakes.rollout("foo", {
    features: [{ featureId: "aboutwelcome", value: { remote: true } }],
  });
  store.addEnrollment(rollout);

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Stored in pref"
  );

  cleanupStorePrefCache();
});

add_task(async function test_syncDataStore_getDefault() {
  cleanupStorePrefCache();
  const store = ExperimentFakes.store();
  const rollout = ExperimentFakes.rollout("aboutwelcome-slug", {
    branch: {
      features: [
        {
          featureId: "aboutwelcome",
          value: { remote: true },
        },
      ],
    },
  });

  await store.init();
  await store.addEnrollment(rollout);

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`)
  );

  let restoredRollout = store.getRolloutForFeature("aboutwelcome");

  Assert.ok(restoredRollout);
  Assert.ok(
    restoredRollout.branch.features[0].value.remote,
    "Restore data from pref"
  );

  cleanupStorePrefCache();
});

add_task(async function test_addEnrollment_rollout() {
  const sandbox = sinon.createSandbox();
  const store = ExperimentFakes.store();
  const stub = sandbox.stub();
  const value = { bar: true };
  let rollout = ExperimentFakes.rollout("foo", {
    features: [{ featureId: "aboutwelcome", value }],
  });

  store._onFeatureUpdate("aboutwelcome", stub);

  await store.init();
  store.addEnrollment(rollout);

  Assert.deepEqual(
    store.getRolloutForFeature("aboutwelcome"),
    rollout,
    "should return the stored value"
  );
  Assert.equal(stub.callCount, 1, "Called once on update");
  Assert.equal(
    stub.firstCall.args[1],
    "rollout-updated",
    "Called for correct reason"
  );
});

add_task(async function test_storeValuePerPref_noVariables() {
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [
        {
          // Ensure it gets saved to prefs
          isEarlyStartup: true,
          featureId: "purple",
        },
      ],
    },
  });

  await store.init();
  store.addEnrollment(experiment);

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
      features: [
        {
          // Ensure it gets saved to prefs
          isEarlyStartup: true,
          featureId: "purple",
          value: { color: "purple", enabled: true },
        },
      ],
    },
  });

  await store.init();
  store.addEnrollment(experiment);

  let branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  let val = Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`);
  Assert.equal(
    val.indexOf("color"),
    -1,
    `Experiment metadata does not contain variables ${val}`
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
      features: [
        {
          // Ensure it gets saved to prefs
          isEarlyStartup: true,
          featureId: "purple",
          value: { color: "purple", enabled: true },
        },
      ],
    },
  });

  await store.init();
  store.addEnrollment(experiment);
  let branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  store = ExperimentFakes.store();
  const cachedExperiment = store.getExperimentForFeature("purple");
  // Cached experiment format only stores early access feature
  cachedExperiment.branch.features = [cachedExperiment.branch.feature];
  delete cachedExperiment.branch.feature;
  Assert.deepEqual(cachedExperiment, experiment, "Returns the same value");

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
      features: [
        {
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
      ],
    },
  });

  await store.init();
  store.addEnrollment(experiment);
  let branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  store = ExperimentFakes.store();
  Assert.deepEqual(
    store.getExperimentForFeature("purple").branch.feature.value,
    experiment.branch.features[0].value,
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
      features: [{ featureId: "purple" }],
    },
  });
  const experiment2 = ExperimentFakes.experiment("bar", {
    branch: {
      slug: "variant",
      features: [{ featureId: "purple" }],
    },
  });
  const experiment3 = ExperimentFakes.experiment("baz", {
    branch: {
      slug: "variant",
      features: [{ featureId: "purple" }],
    },
  });
  const experiment4 = ExperimentFakes.experiment("faz", {
    branch: {
      slug: "variant",
      features: [{ featureId: "purple" }],
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
