"use strict";

const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const COLLECTION_ID_PREF = "messaging-system.rsexperimentloader.collection_id";

/**
 * #getExperiment
 */
add_task(async function test_getExperiment_fromChild_slug() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo");

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => ExperimentFakes.childStore());

  await manager.store.addEnrollment(expected);

  // Wait to sync to child
  await TestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ slug: "foo" }),
    "Wait for child to sync"
  );

  Assert.equal(
    ExperimentAPI.getExperiment({ slug: "foo" }).slug,
    expected.slug,
    "should return an experiment by slug"
  );

  Assert.deepEqual(
    ExperimentAPI.getExperiment({ slug: "foo" }).branch,
    expected.branch,
    "should return the right branch by slug"
  );

  sandbox.restore();
});

add_task(async function test_getExperiment_fromParent_slug() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo");

  await manager.onStartup();
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);
  await ExperimentAPI.ready();

  await manager.store.addEnrollment(expected);

  Assert.equal(
    ExperimentAPI.getExperiment({ slug: "foo" }).slug,
    expected.slug,
    "should return an experiment by slug"
  );

  sandbox.restore();
});

add_task(async function test_getExperimentMetaData() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo");
  let exposureStub = sandbox.stub(ExperimentAPI, "recordExposureEvent");

  await manager.onStartup();
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);
  await ExperimentAPI.ready();

  await manager.store.addEnrollment(expected);

  let metadata = ExperimentAPI.getExperimentMetaData({ slug: expected.slug });

  Assert.equal(
    Object.keys(metadata.branch).length,
    1,
    "Should only expose one property"
  );
  Assert.equal(
    metadata.branch.slug,
    expected.branch.slug,
    "Should have the slug prop"
  );

  Assert.ok(exposureStub.notCalled, "Not called for this method");

  sandbox.restore();
});

add_task(async function test_getRolloutMetaData() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.rollout("foo");
  let exposureStub = sandbox.stub(ExperimentAPI, "recordExposureEvent");

  await manager.onStartup();
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);
  await ExperimentAPI.ready();

  await manager.store.addEnrollment(expected);

  let metadata = ExperimentAPI.getExperimentMetaData({ slug: expected.slug });

  Assert.equal(
    Object.keys(metadata.branch).length,
    1,
    "Should only expose one property"
  );
  Assert.equal(
    metadata.branch.slug,
    expected.branch.slug,
    "Should have the slug prop"
  );

  Assert.ok(exposureStub.notCalled, "Not called for this method");

  sandbox.restore();
});

add_task(function test_getExperimentMetaData_safe() {
  const sandbox = sinon.createSandbox();
  let exposureStub = sandbox.stub(ExperimentAPI, "recordExposureEvent");

  sandbox.stub(ExperimentAPI._store, "get").throws();
  sandbox.stub(ExperimentAPI._store, "getExperimentForFeature").throws();

  try {
    let metadata = ExperimentAPI.getExperimentMetaData({ slug: "foo" });
    Assert.equal(metadata, null, "Should not throw");
  } catch (e) {
    Assert.ok(false, "Error should be caught in ExperimentAPI");
  }

  Assert.ok(ExperimentAPI._store.get.calledOnce, "Sanity check");

  try {
    let metadata = ExperimentAPI.getExperimentMetaData({ featureId: "foo" });
    Assert.equal(metadata, null, "Should not throw");
  } catch (e) {
    Assert.ok(false, "Error should be caught in ExperimentAPI");
  }

  Assert.ok(
    ExperimentAPI._store.getExperimentForFeature.calledOnce,
    "Sanity check"
  );

  Assert.ok(exposureStub.notCalled, "Not called for this feature");

  sandbox.restore();
});

add_task(async function test_getExperiment_feature() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "treatment",
      features: [{ featureId: "cfr", enabled: true, value: null }],
      feature: {
        featureId: "unused-feature-id-for-legacy-support",
        enabled: false,
        value: {},
      },
    },
  });

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => ExperimentFakes.childStore());
  let exposureStub = sandbox.stub(ExperimentAPI, "recordExposureEvent");

  await manager.store.addEnrollment(expected);

  // Wait to sync to child
  await TestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ featureId: "cfr" }),
    "Wait for child to sync"
  );

  Assert.equal(
    ExperimentAPI.getExperiment({ featureId: "cfr" }).slug,
    expected.slug,
    "should return an experiment by featureId"
  );

  Assert.deepEqual(
    ExperimentAPI.getExperiment({ featureId: "cfr" }).branch,
    expected.branch,
    "should return the right branch by featureId"
  );

  Assert.ok(exposureStub.notCalled, "Not called by default");

  sandbox.restore();
});

add_task(async function test_getExperiment_safe() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(ExperimentAPI._store, "getExperimentForFeature").throws();

  try {
    Assert.equal(
      ExperimentAPI.getExperiment({ featureId: "foo" }),
      null,
      "It should not fail even when it throws."
    );
  } catch (e) {
    Assert.ok(false, "Error should be caught by ExperimentAPI");
  }

  sandbox.restore();
});

add_task(async function test_getExperiment_featureAccess() {
  const sandbox = sinon.createSandbox();
  const expected = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "treatment",
      value: { title: "hi" },
      features: [{ featureId: "cfr", value: { message: "content" } }],
    },
  });
  const stub = sandbox
    .stub(ExperimentAPI._store, "getExperimentForFeature")
    .returns(expected);

  let { branch } = ExperimentAPI.getExperiment({ featureId: "cfr" });

  Assert.equal(branch.slug, "treatment");
  let feature = branch.cfr;
  Assert.ok(feature, "Should allow to access by featureId");
  Assert.equal(feature.value.message, "content");

  stub.restore();
});

add_task(async function test_getExperiment_featureAccess_backwardsCompat() {
  const sandbox = sinon.createSandbox();
  const expected = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "treatment",
      feature: { featureId: "cfr", value: { message: "content" } },
    },
  });
  const stub = sandbox
    .stub(ExperimentAPI._store, "getExperimentForFeature")
    .returns(expected);

  let { branch } = ExperimentAPI.getExperiment({ featureId: "cfr" });

  Assert.equal(branch.slug, "treatment");
  let feature = branch.cfr;
  Assert.ok(feature, "Should allow to access by featureId");
  Assert.equal(feature.value.message, "content");

  stub.restore();
});

/**
 * #getRecipe
 */
add_task(async function test_getRecipe() {
  const sandbox = sinon.createSandbox();
  const RECIPE = ExperimentFakes.recipe("foo");
  const collectionName = Services.prefs.getStringPref(COLLECTION_ID_PREF);
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const recipe = await ExperimentAPI.getRecipe("foo");
  Assert.deepEqual(
    recipe,
    RECIPE,
    "should return an experiment recipe if found"
  );
  Assert.equal(
    ExperimentAPI._remoteSettingsClient.collectionName,
    collectionName,
    "Loaded the expected collection"
  );

  sandbox.restore();
});

add_task(async function test_getRecipe_Failure() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").throws();

  const recipe = await ExperimentAPI.getRecipe("foo");
  Assert.equal(recipe, undefined, "should return undefined if RS throws");

  sandbox.restore();
});

/**
 * #getAllBranches
 */
add_task(async function test_getAllBranches() {
  const sandbox = sinon.createSandbox();
  const RECIPE = ExperimentFakes.recipe("foo");
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.deepEqual(
    branches,
    RECIPE.branches,
    "should return all branches if found a recipe"
  );

  sandbox.restore();
});

// API used by Messaging System
add_task(async function test_getAllBranches_featureIdAccessor() {
  const sandbox = sinon.createSandbox();
  const RECIPE = ExperimentFakes.recipe("foo");
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.deepEqual(
    branches,
    RECIPE.branches,
    "should return all branches if found a recipe"
  );
  branches.forEach(branch => {
    Assert.equal(
      branch.testFeature.featureId,
      "testFeature",
      "Should use the experimentBranchAccessor proxy getter"
    );
  });

  sandbox.restore();
});

// For schema version before 1.6.2 branch.feature was accessed
// instead of branch.features
add_task(async function test_getAllBranches_backwardsCompat() {
  const sandbox = sinon.createSandbox();
  const RECIPE = ExperimentFakes.recipe("foo");
  delete RECIPE.branches[0].features;
  delete RECIPE.branches[1].features;
  let feature = {
    featureId: "backwardsCompat",
    enabled: true,
    value: null,
  };
  RECIPE.branches[0].feature = feature;
  RECIPE.branches[1].feature = feature;
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.deepEqual(
    branches,
    RECIPE.branches,
    "should return all branches if found a recipe"
  );
  branches.forEach(branch => {
    Assert.equal(
      branch.backwardsCompat.featureId,
      "backwardsCompat",
      "Should use the experimentBranchAccessor proxy getter"
    );
  });

  sandbox.restore();
});

add_task(async function test_getAllBranches_Failure() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").throws();

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.equal(branches, undefined, "should return undefined if RS throws");

  sandbox.restore();
});

/**
 * #on
 * #off
 */
add_task(async function test_addEnrollment_eventEmit_add() {
  const sandbox = sinon.createSandbox();
  const slugStub = sandbox.stub();
  const featureStub = sandbox.stub();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [{ featureId: "purple", enabled: true, value: null }],
    },
  });
  const store = ExperimentFakes.store();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);

  await store.init();
  await ExperimentAPI.ready();

  ExperimentAPI.on("update", { slug: "foo" }, slugStub);
  ExperimentAPI.on("update", { featureId: "purple" }, featureStub);

  await store.addEnrollment(experiment);

  Assert.equal(
    slugStub.callCount,
    1,
    "should call 'update' callback for slug when experiment is added"
  );
  Assert.equal(slugStub.firstCall.args[1].slug, experiment.slug);
  Assert.equal(
    featureStub.callCount,
    1,
    "should call 'update' callback for featureId when an experiment is added"
  );
  Assert.equal(featureStub.firstCall.args[1].slug, experiment.slug);
});

add_task(async function test_updateExperiment_eventEmit_add_and_update() {
  const sandbox = sinon.createSandbox();
  const slugStub = sandbox.stub();
  const featureStub = sandbox.stub();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [{ featureId: "purple", enabled: true, value: null }],
    },
  });
  const store = ExperimentFakes.store();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);

  await store.init();
  await ExperimentAPI.ready();

  await store.addEnrollment(experiment);

  ExperimentAPI.on("update", { slug: "foo" }, slugStub);
  ExperimentAPI.on("update", { featureId: "purple" }, featureStub);

  store.updateExperiment(experiment.slug, experiment);

  await TestUtils.waitForCondition(
    () => slugStub.callCount == 2,
    "Wait for `on` method to notify callback about the `add` event."
  );
  // Called twice, once when attaching the event listener (because there is an
  // existing experiment with that name) and 2nd time for the update event
  Assert.equal(slugStub.firstCall.args[1].slug, experiment.slug);
  Assert.equal(featureStub.callCount, 2, "Called twice for feature");
  Assert.equal(featureStub.firstCall.args[1].slug, experiment.slug);
});

add_task(async function test_updateExperiment_eventEmit_off() {
  const sandbox = sinon.createSandbox();
  const slugStub = sandbox.stub();
  const featureStub = sandbox.stub();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [{ featureId: "purple", enabled: true, value: null }],
    },
  });
  const store = ExperimentFakes.store();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);

  await store.init();
  await ExperimentAPI.ready();

  ExperimentAPI.on("update", { slug: "foo" }, slugStub);
  ExperimentAPI.on("update", { featureId: "purple" }, featureStub);

  await store.addEnrollment(experiment);

  ExperimentAPI.off("update:foo", slugStub);
  ExperimentAPI.off("update:purple", featureStub);

  store.updateExperiment(experiment.slug, experiment);

  Assert.equal(slugStub.callCount, 1, "Called only once before `off`");
  Assert.equal(featureStub.callCount, 1, "Called only once before `off`");
});

add_task(async function test_getActiveBranch() {
  const sandbox = sinon.createSandbox();
  const store = ExperimentFakes.store();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [{ featureId: "green", enabled: true, value: null }],
    },
  });

  await store.init();
  await store.addEnrollment(experiment);

  Assert.deepEqual(
    ExperimentAPI.getActiveBranch({ featureId: "green" }),
    experiment.branch,
    "Should return feature of active experiment"
  );

  sandbox.restore();
});

add_task(async function test_getActiveBranch_safe() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(ExperimentAPI._store, "getAllActive").throws();

  try {
    Assert.equal(
      ExperimentAPI.getActiveBranch({ featureId: "green" }),
      null,
      "Should not throw"
    );
  } catch (e) {
    Assert.ok(false, "Should catch error in ExperimentAPI");
  }

  sandbox.restore();
});

add_task(async function test_getActiveBranch_storeFailure() {
  const store = ExperimentFakes.store();
  const sandbox = sinon.createSandbox();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [{ featureId: "green", enabled: true }],
    },
  });

  await store.init();
  await store.addEnrollment(experiment);
  // Adding stub later because `addEnrollment` emits update events
  const stub = sandbox.stub(store, "emit");
  // Call getActiveBranch to trigger an activation event
  sandbox.stub(store, "getAllActive").throws();
  try {
    ExperimentAPI.getActiveBranch({ featureId: "green" });
  } catch (e) {
    /* This is expected */
  }

  Assert.equal(stub.callCount, 0, "Not called if store somehow fails");
  sandbox.restore();
});

add_task(async function test_getActiveBranch_noActivationEvent() {
  const store = ExperimentFakes.store();
  const sandbox = sinon.createSandbox();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [{ featureId: "green", enabled: true }],
    },
  });

  await store.init();
  await store.addEnrollment(experiment);
  // Adding stub later because `addEnrollment` emits update events
  const stub = sandbox.stub(store, "emit");
  // Call getActiveBranch to trigger an activation event
  ExperimentAPI.getActiveBranch({ featureId: "green" });

  Assert.equal(stub.callCount, 0, "Not called: sendExposureEvent is false");
  sandbox.restore();
});
