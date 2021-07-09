"use strict";

const {
  ExperimentAPI,
  _ExperimentFeature: ExperimentFeature,
} = ChromeUtils.import("resource://nimbus/ExperimentAPI.jsm");
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

add_task(async function setup() {
  let sandbox = sinon.createSandbox();
  // stub the `observe` method to make sure the Experiment Manager
  // pref listener doesn't trigger and cause side effects
  sandbox.stub(ExperimentManager, "observe");
  await SpecialPowers.pushPrefEnv({
    set: [["app.shield.optoutstudies.enabled", true]],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    sandbox.restore();
  });
});

add_task(async function test_experiment_enroll_unenroll_Telemetry() {
  Services.telemetry.clearEvents();
  const cleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "test-feature",
    value: { enabled: false },
  });
  let experiment = ExperimentAPI.getExperiment({
    featureId: "test-feature",
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
          reason: "cleanup",
          branch: experiment.branch.slug,
          enrollmentId: experiment.enrollmentId,
        },
      },
    ],
    EVENT_FILTER
  );
});

add_task(async function test_experiment_expose_Telemetry() {
  const cleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "test-feature",
    value: { enabled: false },
  });

  let experiment = ExperimentAPI.getExperiment({
    featureId: "test-feature",
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
