"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);
const { CleanupManager } = ChromeUtils.import(
  "resource://normandy/lib/CleanupManager.jsm"
);

const ENABLED_PREF = "messaging-system.rsexperimentloader.enabled";
const RUN_INTERVAL_PREF = "app.normandy.run_interval_seconds";

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
