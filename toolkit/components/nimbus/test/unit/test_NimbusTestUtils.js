"use strict";

const { ExperimentFakes, ExperimentTestUtils } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

add_task(async function test_recipe_fake_validates() {
  const recipe = ExperimentFakes.recipe("foo");
  Assert.ok(
    await ExperimentTestUtils.validateExperiment(recipe),
    "should produce a valid experiment recipe"
  );
});

add_task(async function test_enrollmentHelper() {
  let recipe = ExperimentFakes.recipe("bar");
  recipe.branches.forEach(branch => {
    // Use a feature that will set the sync pref cache
    branch.feature.featureId = "aboutwelcome";
  });
  let manager = ExperimentFakes.manager();

  await manager.onStartup();

  let {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(recipe, { manager });

  await enrollmentPromise;

  Assert.ok(manager.store.getAllActive().length === 1, "Enrolled");
  Assert.equal(
    manager.store.getAllActive()[0].slug,
    recipe.slug,
    "Has expected slug"
  );
  Assert.ok(
    Services.prefs.prefHasUserValue("nimbus.syncdatastore.aboutwelcome"),
    "Sync pref cache set"
  );

  await doExperimentCleanup();

  Assert.ok(manager.store.getAll().length === 0, "Cleanup done");
  Assert.ok(
    !Services.prefs.prefHasUserValue("nimbus.syncdatastore.aboutwelcome"),
    "Sync pref cache is cleared"
  );
});

add_task(async function test_enrollWithFeatureConfig() {
  let manager = ExperimentFakes.manager();
  await manager.onStartup();
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: "enrollWithFeatureConfig",
      value: { enabled: true },
    },
    { manager }
  );

  Assert.ok(
    manager.store.hasExperimentForFeature("enrollWithFeatureConfig"),
    "Enrolled successfully"
  );

  await doExperimentCleanup();

  Assert.ok(
    !manager.store.hasExperimentForFeature("enrollWithFeatureConfig"),
    "Unenrolled successfully"
  );
});
