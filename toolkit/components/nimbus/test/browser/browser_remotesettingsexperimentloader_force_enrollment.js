//creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.importESModule(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { ExperimentManager } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentManager.sys.mjs"
);

async function setup(recipes) {
  const client = RemoteSettings("nimbus-desktop-experiments");
  await client.db.importChanges({}, Date.now(), recipes, {
    clear: true,
  });

  await BrowserTestUtils.waitForCondition(
    async () => (await client.get()).length,
    "RS is ready"
  );

  return {
    client,
    cleanup: () => client.db.clear(),
  };
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["datareporting.healthreport.uploadEnabled", true],
      ["app.shield.optoutstudies.enabled", true],
      ["nimbus.debug", true],
    ],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function test_fetch_recipe_and_branch_no_debug() {
  const sandbox = sinon.createSandbox();
  Services.prefs.setBoolPref("nimbus.debug", false);
  let stub = sandbox.stub(ExperimentManager, "forceEnroll");
  let recipes = [ExperimentFakes.recipe("slug123")];

  const { cleanup } = await setup(recipes);

  await Assert.rejects(
    RemoteSettingsExperimentLoader.optInToExperiment({
      slug: "slug123",
      branch: "control",
    }),
    /Could not opt in/,
    "should throw an error"
  );

  Assert.ok(stub.notCalled, "forceEnroll is not called");

  Services.prefs.setBoolPref("nimbus.debug", true);

  await RemoteSettingsExperimentLoader.optInToExperiment({
    slug: "slug123",
    branch: "control",
  });

  Assert.ok(stub.called, "forceEnroll is called");

  sandbox.restore();
  await cleanup();
});

add_task(async function test_fetch_recipe_and_branch_badslug() {
  const sandbox = sinon.createSandbox();
  let stub = sandbox.stub(ExperimentManager, "forceEnroll");
  let recipes = [ExperimentFakes.recipe("slug123")];

  const { cleanup } = await setup(recipes);

  await Assert.rejects(
    RemoteSettingsExperimentLoader.optInToExperiment({
      slug: "other_slug",
      branch: "control",
    }),
    /Could not find experiment slug other_slug/,
    "should throw an error"
  );

  Assert.ok(stub.notCalled, "forceEnroll is not called");

  sandbox.restore();
  await cleanup();
});

add_task(async function test_fetch_recipe_and_branch_badbranch() {
  const sandbox = sinon.createSandbox();
  let stub = sandbox.stub(ExperimentManager, "forceEnroll");
  let recipes = [ExperimentFakes.recipe("slug123")];

  const { cleanup } = await setup(recipes);

  await Assert.rejects(
    RemoteSettingsExperimentLoader.optInToExperiment({
      slug: "slug123",
      branch: "other_branch",
    }),
    /Could not find branch slug other_branch in slug123/,
    "should throw an error"
  );

  Assert.ok(stub.notCalled, "forceEnroll is not called");

  sandbox.restore();
  await cleanup();
});

add_task(async function test_fetch_recipe_and_branch() {
  const sandbox = sinon.createSandbox();
  let stub = sandbox.stub(ExperimentManager, "forceEnroll");
  let recipes = [ExperimentFakes.recipe("slug_fetch_recipe")];

  const { cleanup } = await setup(recipes);
  await RemoteSettingsExperimentLoader.optInToExperiment({
    slug: "slug_fetch_recipe",
    branch: "control",
  });

  Assert.ok(stub.called, "Called forceEnroll");
  Assert.deepEqual(stub.firstCall.args[0], recipes[0], "Called with recipe");
  Assert.deepEqual(
    stub.firstCall.args[1],
    recipes[0].branches[0],
    "Called with branch"
  );

  sandbox.restore();
  await cleanup();
});

add_task(async function test_invalid_recipe() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub(ExperimentManager, "forceEnroll");
  const recipe = ExperimentFakes.recipe("invalid-recipe");
  delete recipe.branches;

  const { cleanup } = await setup([recipe]);

  await Assert.rejects(
    RemoteSettingsExperimentLoader.optInToExperiment({
      slug: "invalid-recipe",
      branch: "control",
    }),
    /failed validation/
  );

  Assert.ok(stub.notCalled, "forceEnroll not called");

  sandbox.restore();
  await cleanup();
});

add_task(async function test_invalid_branch_variablesOnly() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub(ExperimentManager, "forceEnroll");
  const recipe = ExperimentFakes.recipe("invalid-value");
  recipe.featureIds = ["testFeature"];
  recipe.branches = [recipe.branches[0]];
  recipe.branches[0].features[0].featureId = "testFeature";
  recipe.branches[0].features[0].value = {
    enabled: "foo",
    testInt: true,
    testSetString: 123,
  };

  const { cleanup } = await setup([recipe]);

  await Assert.rejects(
    RemoteSettingsExperimentLoader.optInToExperiment({
      slug: "invalid-value",
      branch: "control",
    }),
    /failed validation/
  );

  Assert.ok(stub.notCalled, "forceEnroll not called");

  sandbox.restore();
  await cleanup();
});

add_task(async function test_invalid_branch_schema() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub(ExperimentManager, "forceEnroll");

  const recipe = ExperimentFakes.recipe("invalid-value");
  recipe.featureIds = ["legacyHeartbeat"];
  recipe.branches = [recipe.branches[0]];
  recipe.branches[0].features[0].featureId = "legacyHeartbeat";
  recipe.branches[0].features[0].value = {
    foo: "bar",
  };

  const { cleanup } = await setup([recipe]);

  await Assert.rejects(
    RemoteSettingsExperimentLoader.optInToExperiment({
      slug: "invalid-value",
      branch: "control",
    }),
    /failed validation/
  );

  Assert.ok(stub.notCalled, "forceEnroll not called");

  sandbox.restore();
  await cleanup();
});

add_task(async function test_invalid_branch_featureId() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub(ExperimentManager, "forceEnroll");
  const recipe = ExperimentFakes.recipe("invalid-value");
  recipe.featureIds = ["UNKNOWN"];
  recipe.branches = [recipe.branches[0]];
  recipe.branches[0].features[0].featureId = "UNKNOWN";

  const { cleanup } = await setup([recipe]);

  await Assert.rejects(
    RemoteSettingsExperimentLoader.optInToExperiment({
      slug: "invalid-value",
      branch: "control",
    }),
    /failed validation/
  );

  Assert.ok(stub.notCalled, "forceEnroll not called");

  sandbox.restore();
  await cleanup();
});
