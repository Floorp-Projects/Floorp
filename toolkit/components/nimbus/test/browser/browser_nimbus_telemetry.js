"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { ExperimentAPI, ExperimentFeature } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const TELEMETRY_CATEGORY = "normandy";
const TELEMETRY_OBJECT = "nimbus_experiment";
// Included with active experiment information
const EXPERIMENT_TYPE = "nimbus";
const EVENT_FILTER = { category: TELEMETRY_CATEGORY };

let rsClient;

add_task(async function setup() {
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

// TODO Use utilities being added
async function addExperiment() {
  const recipe = ExperimentFakes.recipe("foo" + Math.random(), {
    bucketConfig: {
      start: 0,
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

  const cleanup = async () => {
    let waitForExperimentUnenrollment = ExperimentFakes.waitForExperimentUpdate(
      ExperimentAPI,
      { slug: recipe.slug }
    );
    ExperimentManager.unenroll(recipe.slug, "mochitest-cleanup");

    await waitForExperimentUnenrollment;
  };
  return { recipe, cleanup };
}

add_task(async function test_experiment_enroll_unenroll_Telemetry() {
  Services.telemetry.clearEvents();
  const { recipe, cleanup } = await addExperiment();
  let experiment = ExperimentAPI.getExperiment({
    slug: recipe.slug,
  });

  Assert.ok(experiment.branch, "Should be enrolled in the experiment");
  TelemetryTestUtils.assertEvents(
    [
      {
        method: "enroll",
        object: TELEMETRY_OBJECT,
        value: experiment.slug,
        extra: {
          experimentType: EXPERIMENT_TYPE,
          branch: experiment.branch.slug,
          enrollmentId: experiment.enrollmentId,
        },
      },
    ],
    EVENT_FILTER
  );

  await cleanup();

  TelemetryTestUtils.assertEvents(
    [
      {
        method: "unenroll",
        object: TELEMETRY_OBJECT,
        value: experiment.slug,
        extra: {
          reason: "mochitest-cleanup",
          branch: experiment.branch.slug,
          enrollmentId: experiment.enrollmentId,
        },
      },
    ],
    EVENT_FILTER
  );
});

add_task(async function test_experiment_expose_Telemetry() {
  const { recipe, cleanup } = await addExperiment();

  let experiment = ExperimentAPI.getExperiment({
    slug: recipe.slug,
  });

  const { featureId } = experiment.branch.feature;
  const feature = new ExperimentFeature(featureId);

  Services.telemetry.clearEvents();
  feature.getValue({ sendExposureEvent: true });

  TelemetryTestUtils.assertEvents(
    [
      {
        method: "expose",
        object: TELEMETRY_OBJECT,
        value: experiment.slug,
        extra: {
          branchSlug: experiment.branch.slug,
          featureId,
        },
      },
    ],
    EVENT_FILTER
  );

  await cleanup();
});
