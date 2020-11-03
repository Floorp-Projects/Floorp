"use strict";

const { ExperimentAPI } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentAPI.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

/**
 * #getExperiment
 */
add_task(async function test_getExperiment_slug() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo");

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => ExperimentFakes.childStore());

  manager.store.addExperiment(expected);

  // Wait to sync to child
  await TestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ slug: "foo" }),
    "Wait for child to sync"
  );

  Assert.deepEqual(
    ExperimentAPI.getExperiment({ slug: "foo" }),
    expected,
    "should return an experiment by slug"
  );

  sandbox.restore();
});

add_task(async function test_getExperiment_feature() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "treatment",
      value: { title: "hi" },
      feature: { featureId: "cfr", enabled: true },
    },
  });

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => ExperimentFakes.childStore());

  manager.store.addExperiment(expected);

  // Wait to sync to child
  await TestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ featureId: "cfr" }),
    "Wait for child to sync"
  );

  Assert.deepEqual(
    ExperimentAPI.getExperiment({ featureId: "cfr" }),
    expected,
    "should return an experiment by slug"
  );

  sandbox.restore();
});

/**
 * #getValue
 */
add_task(async function test_getValue() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const feature = {
    featureId: "aboutwelcome",
    enabled: true,
    value: { title: "hi" },
  };
  const expected = ExperimentFakes.experiment("foo", {
    branch: { slug: "treatment", feature },
  });

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => ExperimentFakes.childStore());

  manager.store.addExperiment(expected);

  await TestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ slug: "foo" }),
    "Wait for child to sync"
  );

  Assert.deepEqual(
    ExperimentAPI.getFeatureValue("aboutwelcome"),
    feature.value,
    "should return an experiment value by slug"
  );

  Assert.equal(
    ExperimentAPI.getFeatureValue("doesnotexist"),
    undefined,
    "should return undefined if the experiment is not found"
  );

  sandbox.restore();
});

/**
 * #isFeatureEnabled
 */

add_task(async function test_isFeatureEnabledDefault() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const FEATURE_ENABLED_DEFAULT = true;
  const expected = ExperimentFakes.experiment("foo");

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  manager.store.addExperiment(expected);

  Assert.deepEqual(
    ExperimentAPI.isFeatureEnabled("aboutwelcome", FEATURE_ENABLED_DEFAULT),
    FEATURE_ENABLED_DEFAULT,
    "should return enabled true as default"
  );
  sandbox.restore();
});

add_task(async function test_isFeatureEnabled() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const feature = {
    featureId: "aboutwelcome",
    enabled: false,
    value: null,
  };
  const expected = ExperimentFakes.experiment("foo", {
    branch: { slug: "treatment", feature },
  });

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  manager.store.addExperiment(expected);

  Assert.deepEqual(
    ExperimentAPI.isFeatureEnabled("aboutwelcome", true),
    feature.enabled,
    "should return feature as disabled"
  );
  sandbox.restore();
});

/**
 * #getRecipe
 */
add_task(async function test_getRecipe() {
  const sandbox = sinon.createSandbox();
  const RECIPE = ExperimentFakes.recipe("foo");
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const recipe = await ExperimentAPI.getRecipe("foo");
  Assert.deepEqual(
    recipe,
    RECIPE,
    "should return an experiment recipe if found"
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
add_task(async function test_addExperiment_eventEmit_add() {
  const sandbox = sinon.createSandbox();
  const slugStub = sandbox.stub();
  const featureStub = sandbox.stub();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "purple", enabled: true },
    },
  });
  const store = ExperimentFakes.store();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);

  await store.init();
  await ExperimentAPI.ready();

  ExperimentAPI.on("update", { slug: "foo" }, slugStub);
  ExperimentAPI.on("update", { featureId: "purple" }, featureStub);

  store.addExperiment(experiment);

  Assert.equal(slugStub.callCount, 1);
  Assert.equal(slugStub.firstCall.args[1].slug, experiment.slug);
  Assert.equal(featureStub.callCount, 1);
  Assert.equal(featureStub.firstCall.args[1].slug, experiment.slug);
});

add_task(async function test_updateExperiment_eventEmit_add_and_update() {
  const sandbox = sinon.createSandbox();
  const slugStub = sandbox.stub();
  const featureStub = sandbox.stub();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      feature: { featureId: "purple", enabled: true },
    },
  });
  const store = ExperimentFakes.store();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);

  await store.init();
  await ExperimentAPI.ready();

  store.addExperiment(experiment);

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
      feature: { featureId: "purple", enabled: true },
    },
  });
  const store = ExperimentFakes.store();
  sandbox.stub(ExperimentAPI, "_store").get(() => store);

  await store.init();
  await ExperimentAPI.ready();

  ExperimentAPI.on("update", { slug: "foo" }, slugStub);
  ExperimentAPI.on("update", { featureId: "purple" }, featureStub);

  store.addExperiment(experiment);

  ExperimentAPI.off("update:foo", slugStub);
  ExperimentAPI.off("update:purple", featureStub);

  store.updateExperiment(experiment.slug, experiment);

  Assert.equal(slugStub.callCount, 1, "Called only once before `off`");
  Assert.equal(featureStub.callCount, 1, "Called only once before `off`");
});
