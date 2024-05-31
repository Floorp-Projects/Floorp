"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { ExperimentManager } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentManager.sys.mjs"
);
const { RemoteSettingsExperimentLoader, EnrollmentsContext } =
  ChromeUtils.importESModule(
    "resource://nimbus/lib/RemoteSettingsExperimentLoader.sys.mjs"
  );
const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const ENABLED_PREF = "messaging-system.rsexperimentloader.enabled";
const RUN_INTERVAL_PREF = "app.normandy.run_interval_seconds";
const STUDIES_OPT_OUT_PREF = "app.shield.optoutstudies.enabled";
const UPLOAD_PREF = "datareporting.healthreport.uploadEnabled";
const DEBUG_PREF = "nimbus.debug";

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
  ok(loader.updateRecipes.calledOnce, "should call .updatpickeRecipes");
});

add_task(async function test_init_with_opt_in() {
  const loader = ExperimentFakes.rsLoader();
  sinon.stub(loader, "setTimer");
  sinon.stub(loader, "updateRecipes").resolves();

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, false);
  await loader.init();
  equal(
    loader.setTimer.callCount,
    0,
    `should not initialize if ${STUDIES_OPT_OUT_PREF} pref is false`
  );

  Services.prefs.setBoolPref(ENABLED_PREF, false);
  await loader.init();
  equal(
    loader.setTimer.callCount,
    0,
    `should not initialize if ${ENABLED_PREF} pref is false`
  );

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);
  Services.prefs.setBoolPref(ENABLED_PREF, true);
  await loader.init();
  ok(loader.setTimer.calledOnce, "should call .setTimer");
  ok(loader.updateRecipes.calledOnce, "should call .updateRecipes");
});

add_task(async function test_updateRecipes() {
  const loader = ExperimentFakes.rsLoader();

  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: "true",
  });
  const FAIL_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: "false",
  });
  sinon.stub(loader, "setTimer");
  sinon.spy(loader, "updateRecipes");

  sinon
    .stub(loader.remoteSettingsClients.experiments, "get")
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
    loader.manager.onRecipe.calledWith(PASS_FILTER_RECIPE, "rs-loader"),
    "should call .onRecipe with argument data"
  );
});

add_task(async function test_updateRecipes_someMismatch() {
  const loader = ExperimentFakes.rsLoader();

  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: "true",
  });
  const FAIL_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: "false",
  });
  sinon.stub(loader, "setTimer");
  sinon.spy(loader, "updateRecipes");

  sinon
    .stub(loader.remoteSettingsClients.experiments, "get")
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
  ok(loader.manager.onFinalize.calledOnce, "Should call onFinalize.");
  ok(
    onFinalizeCalled(loader.manager.onFinalize, "rs-loader", {
      recipeMismatches: [FAIL_FILTER_RECIPE.slug],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      missingL10nIds: new Map(),
      missingLocale: [],
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "should call .onFinalize with the recipes that failed targeting"
  );
});

add_task(async function test_updateRecipes_forFirstStartup() {
  const loader = ExperimentFakes.rsLoader();
  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: "isFirstStartup",
  });
  sinon
    .stub(loader.remoteSettingsClients.experiments, "get")
    .resolves([PASS_FILTER_RECIPE]);
  sinon.stub(loader.manager, "onRecipe").resolves();
  sinon.stub(loader.manager, "onFinalize");
  sinon
    .stub(loader.manager, "createTargetingContext")
    .returns({ isFirstStartup: true });

  Services.prefs.setBoolPref(ENABLED_PREF, true);
  await loader.init({ isFirstStartup: true });

  ok(loader.manager.onRecipe.calledOnce, "should pass the targeting filter");
});

add_task(async function test_updateRecipes_forNoneFirstStartup() {
  const loader = ExperimentFakes.rsLoader();
  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: "isFirstStartup",
  });
  sinon
    .stub(loader.remoteSettingsClients.experiments, "get")
    .resolves([PASS_FILTER_RECIPE]);
  sinon.stub(loader.manager, "onRecipe").resolves();
  sinon.stub(loader.manager, "onFinalize");
  sinon
    .stub(loader.manager, "createTargetingContext")
    .returns({ isFirstStartup: false });

  Services.prefs.setBoolPref(ENABLED_PREF, true);
  await loader.init({ isFirstStartup: true });

  ok(loader.manager.onRecipe.notCalled, "should not pass the targeting filter");
});

add_task(async function test_checkTargeting() {
  const loader = ExperimentFakes.rsLoader();
  const ctx = new EnrollmentsContext(loader.manager);
  equal(
    await ctx.checkTargeting({}),
    true,
    "should return true if .targeting is not defined"
  );
  equal(
    await ctx.checkTargeting({
      targeting: "'foo'",
      slug: "test_checkTargeting",
    }),
    true,
    "should return true for truthy expression"
  );
  equal(
    await ctx.checkTargeting({
      targeting: "aPropertyThatDoesNotExist",
      slug: "test_checkTargeting",
    }),
    false,
    "should return false for falsey expression"
  );
});

add_task(async function test_checkExperimentSelfReference() {
  const loader = ExperimentFakes.rsLoader();
  const ctx = new EnrollmentsContext(loader.manager);
  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting:
      "experiment.slug == 'foo' && experiment.branches[0].slug == 'control'",
  });

  const FAIL_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: "experiment.slug == 'bar'",
  });

  equal(
    await ctx.checkTargeting(PASS_FILTER_RECIPE),
    true,
    "Should return true for matching on slug name and branch"
  );
  equal(
    await ctx.checkTargeting(FAIL_FILTER_RECIPE),
    false,
    "Should fail targeting"
  );
});

add_task(async function test_optIn_debug_disabled() {
  info("Testing users cannot opt-in when nimbus.debug is false");

  const loader = ExperimentFakes.rsLoader();
  sinon.stub(loader, "setTimer");
  sinon.stub(loader, "updateRecipes").resolves();

  const recipe = ExperimentFakes.recipe("foo");
  sinon
    .stub(loader.remoteSettingsClients.experiments, "get")
    .resolves([recipe]);

  Services.prefs.setBoolPref(DEBUG_PREF, false);
  Services.prefs.setBoolPref(UPLOAD_PREF, true);
  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);

  await Assert.rejects(
    loader.optInToExperiment({
      slug: recipe.slug,
      branchSlug: recipe.branches[0].slug,
    }),
    /Could not opt in/
  );

  Services.prefs.clearUserPref(DEBUG_PREF);
  Services.prefs.clearUserPref(UPLOAD_PREF);
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
});

add_task(async function test_optIn_studies_disabled() {
  info(
    "Testing users cannot opt-in when telemetry is disabled or studies are disabled."
  );

  const prefs = [UPLOAD_PREF, STUDIES_OPT_OUT_PREF];

  const loader = ExperimentFakes.rsLoader();
  sinon.stub(loader, "setTimer");
  sinon.stub(loader, "updateRecipes").resolves();

  const recipe = ExperimentFakes.recipe("foo");
  sinon
    .stub(loader.remoteSettingsClients.experiments, "get")
    .resolves([recipe]);

  Services.prefs.setBoolPref(DEBUG_PREF, true);

  for (const pref of prefs) {
    Services.prefs.setBoolPref(UPLOAD_PREF, true);
    Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);

    Services.prefs.setBoolPref(pref, false);

    await Assert.rejects(
      loader.optInToExperiment({
        slug: recipe.slug,
        branchSlug: recipe.branches[0].slug,
      }),
      /Could not opt in: studies are disabled/
    );
  }

  Services.prefs.clearUserPref(DEBUG_PREF);
  Services.prefs.clearUserPref(UPLOAD_PREF);
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
});

add_task(async function test_enrollment_changed_notification() {
  const loader = ExperimentFakes.rsLoader();

  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: "true",
  });
  sinon.stub(loader, "setTimer");
  sinon.spy(loader, "updateRecipes");
  const enrollmentChanged = TestUtils.topicObserved(
    "nimbus:enrollments-updated"
  );
  sinon
    .stub(loader.remoteSettingsClients.experiments, "get")
    .resolves([PASS_FILTER_RECIPE]);
  sinon.stub(loader.manager, "onRecipe").resolves();
  sinon.stub(loader.manager, "onFinalize");

  Services.prefs.setBoolPref(ENABLED_PREF, true);
  await loader.init();
  await enrollmentChanged;
  ok(loader.updateRecipes.called, "should call .updateRecipes");
});

add_task(async function test_experiment_optin_targeting() {
  Services.prefs.setBoolPref(DEBUG_PREF, true);

  const sandbox = sinon.createSandbox();

  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  await loader.init();
  await manager.onStartup();
  await manager.store.ready();

  const recipe = ExperimentFakes.recipe("foo", { targeting: "false" });

  sandbox.stub(RemoteSettings("nimbus-preview"), "get").resolves([recipe]);

  await Assert.rejects(
    loader.optInToExperiment({
      slug: recipe.slug,
      branch: recipe.branches[0].slug,
      collection: "nimbus-preview",
      applyTargeting: true,
    }),
    /Recipe foo did not match targeting/,
    "optInToExperiment should throw"
  );

  Assert.ok(
    !manager.store.getExperimentForFeature("testFeature"),
    "Should not enroll in experiment"
  );

  await loader.optInToExperiment({
    slug: recipe.slug,
    branch: recipe.branches[0].slug,
    collection: "nimbus-preview",
  });

  Assert.equal(
    manager.store.getExperimentForFeature("testFeature").slug,
    `optin-${recipe.slug}`,
    "Should enroll in experiment"
  );

  manager.unenroll(`optin-${recipe.slug}`, "test-cleanup");

  sandbox.restore();
  Services.prefs.clearUserPref(DEBUG_PREF);

  await assertEmptyStore(manager.store, { cleanup: true });
});
