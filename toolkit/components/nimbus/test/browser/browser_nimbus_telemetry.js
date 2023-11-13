"use strict";

const { ExperimentAPI, _ExperimentFeature: ExperimentFeature } =
  ChromeUtils.importESModule("resource://nimbus/ExperimentAPI.sys.mjs");
const { ExperimentManager } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentManager.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const TELEMETRY_CATEGORY = "normandy";
const TELEMETRY_OBJECT = "nimbus_experiment";
// Included with active experiment information
const EXPERIMENT_TYPE = "nimbus";
const EVENT_FILTER = { category: TELEMETRY_CATEGORY };

add_setup(async function () {
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
        },
      },
    ],
    EVENT_FILTER
  );
});

add_task(async function test_experiment_expose_Telemetry() {
  const featureManifest = {
    description: "Test feature",
    exposureDescription: "Used in tests",
  };
  const cleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "test-feature",
    value: { enabled: false },
  });

  let experiment = ExperimentAPI.getExperiment({
    featureId: "test-feature",
  });

  const { featureId } = experiment.branch.features[0];
  const feature = new ExperimentFeature(featureId, featureManifest);

  Services.telemetry.clearEvents();
  feature.recordExposureEvent();

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

add_task(async function test_rollout_expose_Telemetry() {
  const featureManifest = {
    description: "Test feature",
    exposureDescription: "Used in tests",
  };
  const cleanup = await ExperimentFakes.enrollWithRollout({
    featureId: "test-feature",
    value: { enabled: false },
  });

  let rollout = ExperimentAPI.getRolloutMetaData({
    featureId: "test-feature",
  });

  Assert.ok(rollout.slug, "Found enrolled experiment");

  const feature = new ExperimentFeature("test-feature", featureManifest);

  Services.telemetry.clearEvents();
  feature.recordExposureEvent();

  TelemetryTestUtils.assertEvents(
    [
      {
        method: "expose",
        object: TELEMETRY_OBJECT,
        value: rollout.slug,
        extra: {
          branchSlug: rollout.branch.slug,
          featureId: feature.featureId,
        },
      },
    ],
    EVENT_FILTER
  );

  await cleanup();
});
