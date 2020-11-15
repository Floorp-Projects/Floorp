"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);

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
});

add_task(async function test_event_updates_main() {
  const sandbox = sinon.createSandbox();
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo");
  const updateEventCbStub = sandbox.stub();

  // Setup ExperimentManager and child store for ExperimentAPI
  await store.init();

  // Set update cb
  store.on("update:aboutwelcome", updateEventCbStub);

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

add_task(async function test_recordExposureEvent() {
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");
  const experimentData = {
    experimentSlug: experiment.slug,
    branchSlug: experiment.branch.slug,
    featureId: experiment.branch.feature.featureId,
  };
  await manager.onStartup();

  let exposureEvEmit = new Promise(resolve =>
    manager.store.on("exposure", (ev, data) => resolve(data))
  );

  manager.store.addExperiment(experiment);
  manager.store._emitExperimentExposure(experimentData);

  let result = await exposureEvEmit;

  Assert.deepEqual(
    result,
    experimentData,
    "should return the same data as sent"
  );
});

add_task(async function test_activateBranch() {
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "green", enabled: true },
    },
  });

  await store.init();
  store.addExperiment(experiment);

  Assert.deepEqual(
    store.activateBranch({ featureId: "green" }),
    experiment.branch,
    "Should return feature of active experiment"
  );
});

add_task(async function test_activateBranch_activationEvent() {
  const store = ExperimentFakes.store();
  const sandbox = sinon.createSandbox();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "green", enabled: true },
    },
  });

  await store.init();
  store.addExperiment(experiment);
  // Adding stub later because `addExperiment` emits update events
  const stub = sandbox.stub(store, "emit");
  // Call activateBranch to trigger an activation event
  store.activateBranch({ featureId: "green" });

  Assert.equal(stub.callCount, 1, "Called by doing activateBranch");
  Assert.equal(stub.firstCall.args[0], "exposure", "Has correct event name");
  Assert.equal(
    stub.firstCall.args[1].experimentSlug,
    experiment.slug,
    "Has correct payload"
  );
});

add_task(async function test_activateBranch_storeFailure() {
  const store = ExperimentFakes.store();
  const sandbox = sinon.createSandbox();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "green", enabled: true },
    },
  });

  await store.init();
  store.addExperiment(experiment);
  // Adding stub later because `addExperiment` emits update events
  const stub = sandbox.stub(store, "emit");
  // Call activateBranch to trigger an activation event
  sandbox.stub(store, "getAllActive").throws();
  try {
    store.activateBranch({ featureId: "green" });
  } catch (e) {
    /* This is expected */
  }

  Assert.equal(stub.callCount, 0, "Not called if store somehow fails");
});

add_task(async function test_activateBranch_noActivationEvent() {
  const store = ExperimentFakes.store();
  const sandbox = sinon.createSandbox();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "green", enabled: true },
    },
  });

  await store.init();
  store.addExperiment(experiment);
  // Adding stub later because `addExperiment` emits update events
  const stub = sandbox.stub(store, "emit");
  // Call activateBranch to trigger an activation event
  store.activateBranch({ featureId: "green", sendExposurePing: false });

  Assert.equal(stub.callCount, 0, "Not called: sendExposurePing is false");
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
