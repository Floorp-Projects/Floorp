"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

let rsClient;

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["app.shield.optoutstudies.enabled", true],
    ],
  });
  rsClient = RemoteSettings("nimbus-desktop-experiments");

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
    { slug: recipe.slug }
  );
  RemoteSettingsExperimentLoader.updateRecipes("mochitest");

  await waitForExperimentEnrollment;

  let experiment = ExperimentAPI.getExperiment({
    slug: recipe.slug,
  });

  Assert.ok(experiment.active, "Should be enrolled in the experiment");

  let waitForExperimentUnenrollment = ExperimentFakes.waitForExperimentUpdate(
    ExperimentAPI,
    { slug: recipe.slug }
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
