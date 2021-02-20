"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { CleanupManager } = ChromeUtils.import(
  "resource://normandy/lib/CleanupManager.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { FirstStartup } = ChromeUtils.import(
  "resource://gre/modules/FirstStartup.jsm"
);

add_task(async function test_updateRecipes_activeExperiments() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const recipe = ExperimentFakes.recipe("foo");
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;
  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: `"${recipe.slug}" in activeExperiments`,
  });
  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([PASS_FILTER_RECIPE]);
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActive").returns([recipe]);

  await loader.init();

  ok(onRecipe.calledOnce, "Should match active experiments");
});

add_task(async function test_updateRecipes_isFirstRun() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const recipe = ExperimentFakes.recipe("foo");
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;
  const PASS_FILTER_RECIPE = { ...recipe, targeting: "isFirstStartup" };
  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([PASS_FILTER_RECIPE]);
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActive").returns([recipe]);

  // Pretend to be in the first startup
  FirstStartup._state = FirstStartup.IN_PROGRESS;
  await loader.init();

  Assert.ok(onRecipe.calledOnce, "Should match first run");
});
