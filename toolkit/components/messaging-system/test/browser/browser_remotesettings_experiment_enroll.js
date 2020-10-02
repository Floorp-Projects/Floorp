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

const TEST_EXPERIMENT = {
  enabled: true,
  arguments: {
    slug: "bug-1632140-end-to-end-mochitest-for-enrollment",
    active: true,
    experimentType: "mochitest",
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        feature: { featureId: "treatment", enabled: true, value: null },
      },
      {
        slug: "control",
        ratio: 1,
        feature: { featureId: "control", enabled: true, value: null },
      },
    ],
    bucketConfig: {
      count: 100,
      start: 0,
      total: 100,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
    userFacingName: "Test beep beep",
    referenceBranch: "control",
    isEnrollmentPaused: false,
    proposedEnrollment: 7,
    userFacingDescription: "This is a Mochitest",
  },
  targeting: "true",
  id: "bug-1632140-end-to-end-mochitest-for-enrollment",
};
let rsClient;

function createTestExperiment() {
  return {
    ...TEST_EXPERIMENT,
    arguments: {
      ...TEST_EXPERIMENT.arguments,
      slug: TEST_EXPERIMENT.arguments.slug + Date.now(),
    },
    id: TEST_EXPERIMENT.id + Date.now(),
  };
}

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
  const recipe = createTestExperiment();
  await rsClient.db.importChanges({}, 42, [recipe], {
    clear: true,
  });

  let waitForExperimentEnrollment = ExperimentFakes.waitForExperimentUpdate(
    ExperimentAPI,
    recipe.arguments.slug
  );
  RemoteSettingsExperimentLoader.updateRecipes("mochitest");

  await waitForExperimentEnrollment;

  let experiment = ExperimentAPI.getExperiment({
    slug: recipe.arguments.slug,
  });

  Assert.ok(experiment.active, "Should be enrolled in the experiment");

  let waitForExperimentUnenrollment = ExperimentFakes.waitForExperimentUpdate(
    ExperimentAPI,
    recipe.arguments.slug
  );
  ExperimentManager.unenroll(recipe.arguments.slug, "mochitest-cleanup");

  await waitForExperimentUnenrollment;

  experiment = ExperimentAPI.getExperiment({
    slug: recipe.arguments.slug,
  });

  Assert.ok(!experiment.active, "Experiment is no longer active");
});
