"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);
const { CleanupManager } = ChromeUtils.import(
  "resource://normandy/lib/CleanupManager.jsm"
);

const { ExperimentManager } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentManager.jsm"
);

const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://messaging-system/lib/RemoteSettingsExperimentLoader.jsm"
);

const ENABLED_PREF = "messaging-system.rsexperimentloader.enabled";
const RUN_INTERVAL_PREF = "app.normandy.run_interval_seconds";

add_task(async function test_real_exp_manager() {
  equal(
    RemoteSettingsExperimentLoader.manager,
    ExperimentManager,
    "should reference ExperimentManager singleton by default"
  );
});

add_task(async function test_lazy_pref_getters() {
  const loader = ExperimentFakes.rsLoader();
  sinon.stub(loader, "updateRecipes").resolves();

  Services.prefs.setIntPref(RUN_INTERVAL_PREF, 123456);
  equal(
    loader.intervalInSeconds,
    123456,
    `should set intervalInSeconds to the value of ${RUN_INTERVAL_PREF}`
  );

  Services.prefs.setBoolPref(ENABLED_PREF, true);
  equal(
    loader.enabled,
    true,
    `should set enabled to the value of ${ENABLED_PREF}`
  );
  Services.prefs.setBoolPref(ENABLED_PREF, false);
  equal(loader.enabled, false);

  Services.prefs.clearUserPref(RUN_INTERVAL_PREF);
  Services.prefs.clearUserPref(ENABLED_PREF);
});

add_task(async function test_init() {
  const loader = ExperimentFakes.rsLoader();
  sinon.stub(loader, "setTimer");
  sinon.stub(loader, "updateRecipes").resolves();

  Services.prefs.setBoolPref(ENABLED_PREF, false);
  await loader.init();
  equal(
    loader.setTimer.callCount,
    0,
    `should not initialize if ${ENABLED_PREF} pref is false`
  );

  Services.prefs.setBoolPref(ENABLED_PREF, true);
  await loader.init();
  ok(loader.setTimer.calledOnce, "should call .setTimer");
  ok(loader.updateRecipes.calledOnce, "should call .updateRecipes");
});

add_task(async function test_updateRecipes() {
  const loader = ExperimentFakes.rsLoader();
  const PASS_FILTER_RECIPE = {
    targeting: "true",
    arguments: ExperimentFakes.recipe("foo"),
  };
  const FAIL_FILTER_RECIPE = {
    targeting: "false",
    arguments: ExperimentFakes.recipe("foo"),
  };
  sinon.stub(loader, "setTimer");
  sinon.spy(loader, "updateRecipes");

  sinon
    .stub(loader.remoteSettingsClient, "get")
    .resolves([PASS_FILTER_RECIPE, FAIL_FILTER_RECIPE]);
  sinon.stub(loader.manager, "onRecipe").resolves();
  sinon.stub(loader.manager, "onFinalize");

  Services.prefs.setBoolPref(ENABLED_PREF, true);
  await loader.init();
  ok(loader.updateRecipes.calledOnce, "should call .updateRecipes");
  equal(
    loader.manager.onRecipe.callCount,
    1,
    "should call .onRecipe only for recipes that pass"
  );
  ok(
    loader.manager.onRecipe.calledWith(
      PASS_FILTER_RECIPE.arguments,
      "rs-loader"
    ),
    "should call .onRecipe with argument data"
  );
});

add_task(async function test_checkTargeting() {
  const loader = ExperimentFakes.rsLoader();
  equal(
    await loader.checkTargeting({}),
    true,
    "should return true if .targeting is not defined"
  );
  equal(
    await loader.checkTargeting({ targeting: "'foo'" }),
    true,
    "should return true for truthy expression"
  );
  equal(
    await loader.checkTargeting({ targeting: "aPropertyThatDoesNotExist" }),
    false,
    "should return false for falsey expression"
  );
});
