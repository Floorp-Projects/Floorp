"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://messaging-system/lib/RemoteSettingsExperimentLoader.jsm"
);
const { ExperimentAPI } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentAPI.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentManager.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);

let rsClient;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["messaging-system.log", "all"]],
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
  const recipe = ExperimentFakes.recipe("foo" + Date.now(), {
    bucketConfig: {
      start: 0,
      // Make sure the experiment enrolls
      count: 10000,
      total: 10000,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
  });
  await rsClient.db.importChanges({}, 42, [recipe], {
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
});
