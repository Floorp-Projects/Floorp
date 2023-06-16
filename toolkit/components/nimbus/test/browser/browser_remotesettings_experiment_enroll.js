"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.importESModule(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.sys.mjs"
);
const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentManager } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentManager.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

let rsClient;

add_setup(async function () {
  rsClient = RemoteSettings("nimbus-desktop-experiments");
  await rsClient.db.importChanges({}, Date.now(), [], { clear: true });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["datareporting.healthreport.uploadEnabled", true],
      ["app.shield.optoutstudies.enabled", true],
    ],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    await rsClient.db.clear();
  });
});

add_task(async function test_experimentEnrollment() {
  // Need to randomize the slug so subsequent test runs don't skip enrollment
  // due to a conflicting slug
  const recipe = ExperimentFakes.recipe("foo" + Math.random(), {
    bucketConfig: {
      start: 0,
      // Make sure the experiment enrolls
      count: 10000,
      total: 10000,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
  });
  await rsClient.db.importChanges({}, Date.now(), [recipe], {
    clear: true,
  });

  let waitForExperimentEnrollment = ExperimentFakes.waitForExperimentUpdate(
    ExperimentAPI,
    recipe.slug
  );
  RemoteSettingsExperimentLoader.updateRecipes("mochitest");

  await waitForExperimentEnrollment;

  let experiment = ExperimentAPI.getExperiment({
    slug: recipe.slug,
  });

  Assert.ok(experiment.active, "Should be enrolled in the experiment");

  let waitForExperimentUnenrollment = ExperimentFakes.waitForExperimentUpdate(
    ExperimentAPI,
    recipe.slug
  );
  ExperimentManager.unenroll(recipe.slug, "mochitest-cleanup");

  await waitForExperimentUnenrollment;

  experiment = ExperimentAPI.getExperiment({
    slug: recipe.slug,
  });

  Assert.ok(!experiment.active, "Experiment is no longer active");
  ExperimentAPI._store._deleteForTests(recipe.slug);
});

add_task(async function test_experimentEnrollment_startup() {
  // Studies pref can turn the feature off but if the feature pref is off
  // then it stays off.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.rsexperimentloader.enabled", false],
      ["app.shield.optoutstudies.enabled", false],
    ],
  });

  Assert.ok(!RemoteSettingsExperimentLoader.enabled, "Should be disabled");

  await SpecialPowers.pushPrefEnv({
    set: [["app.shield.optoutstudies.enabled", true]],
  });

  Assert.ok(
    !RemoteSettingsExperimentLoader.enabled,
    "Should still be disabled (feature pref is off)"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["messaging-system.rsexperimentloader.enabled", true]],
  });

  Assert.ok(
    RemoteSettingsExperimentLoader.enabled,
    "Should finally be enabled"
  );
});
