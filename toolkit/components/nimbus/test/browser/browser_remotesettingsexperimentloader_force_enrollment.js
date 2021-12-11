/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const {
  RemoteDefaultsLoader,
  RemoteSettingsExperimentLoader,
} = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);

async function setup(recipes) {
  const client = RemoteSettings("nimbus-desktop-experiments");
  await client.db.importChanges({}, 42, recipes, {
    clear: true,
  });

  await BrowserTestUtils.waitForCondition(
    async () => (await client.get()).length,
    "RS is ready"
  );

  registerCleanupFunction(async () => {
    await client.db.clear();
  });

  return client;
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
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
  let stub = sandbox.stub(ExperimentManager, "forceEnroll").returns(true);
  let recipes = [ExperimentFakes.recipe("slug123")];

  await setup(recipes);

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

  const result = await RemoteSettingsExperimentLoader.optInToExperiment({
    slug: "slug123",
    branch: "control",
  });

  Assert.ok(result, "Pref was turned on");
  Assert.ok(stub.called, "forceEnroll is called");

  sandbox.restore();
});

add_task(async function test_fetch_recipe_and_branch_badslug() {
  const sandbox = sinon.createSandbox();
  let stub = sandbox.stub(ExperimentManager, "forceEnroll").returns(true);
  let recipes = [ExperimentFakes.recipe("slug123")];

  await setup(recipes);

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
});

add_task(async function test_fetch_recipe_and_branch_badbranch() {
  const sandbox = sinon.createSandbox();
  let stub = sandbox.stub(ExperimentManager, "forceEnroll").returns(true);
  let recipes = [ExperimentFakes.recipe("slug123")];

  await setup(recipes);

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
});

add_task(async function test_fetch_recipe_and_branch() {
  const sandbox = sinon.createSandbox();
  let stub = sandbox.stub(ExperimentManager, "forceEnroll").returns(true);
  let recipes = [ExperimentFakes.recipe("slug_fetch_recipe")];

  await setup(recipes);
  let result = await RemoteSettingsExperimentLoader.optInToExperiment({
    slug: "slug_fetch_recipe",
    branch: "control",
  });

  Assert.ok(result, "Recipe found");
  Assert.ok(stub.called, "Called forceEnroll");
  Assert.deepEqual(stub.firstCall.args[0], recipes[0], "Called with recipe");
  Assert.deepEqual(
    stub.firstCall.args[1],
    recipes[0].branches[0],
    "Called with branch"
  );

  sandbox.restore();
});
