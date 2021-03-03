/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  RemoteSettingsExperimentLoader,
  _RemoteSettingsExperimentLoader,
} = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

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

add_task(async function test_double_feature_enrollment() {
  let sandbox = sinon.createSandbox();
  // We want to prevent this because it would start a recipe
  // update outside of our asserts
  sandbox.stub(RemoteSettingsExperimentLoader, "setTimer");
  let sendFailureTelemetryStub = sandbox.stub(
    ExperimentManager,
    "sendFailureTelemetry"
  );

  for (let experiment of ExperimentManager.store.getAllActive()) {
    ExperimentManager.unenroll(experiment.slug, "cleanup");
    ExperimentManager.store._deleteForTests(experiment.slug);
  }
  await BrowserTestUtils.waitForCondition(
    () => ExperimentManager.store.getAllActive().length === 0
  );

  const recipe1 = ExperimentFakes.recipe("foo" + Date.now(), {
    bucketConfig: {
      start: 0,
      // Make sure the experiment enrolls
      count: 10000,
      total: 10000,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
  });
  const recipe2 = ExperimentFakes.recipe("foo" + Date.now(), {
    bucketConfig: {
      start: 0,
      // Make sure the experiment enrolls
      count: 10000,
      total: 10000,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
  });

  await rsClient.db.importChanges({}, 42, [recipe1, recipe2], {
    clear: true,
  });

  RemoteSettingsExperimentLoader.uninit();
  let enrolledPromise = new Promise(resolve =>
    ExperimentManager.store.on("update:test-feature", resolve)
  );
  await RemoteSettingsExperimentLoader.init();
  await enrolledPromise;

  Assert.ok(
    RemoteSettingsExperimentLoader._initialized,
    "It should initialize and process the recipes"
  );

  BrowserTestUtils.waitForCondition(
    () => sendFailureTelemetryStub.calledOnce,
    "Expected to fail one of the recipes"
  );

  for (let experiment of ExperimentManager.store.getAllActive()) {
    ExperimentManager.unenroll(experiment.slug, "cleanup");
    ExperimentManager.store._deleteForTests(experiment.slug);
  }
  await BrowserTestUtils.waitForCondition(
    () => ExperimentManager.store.getAllActive().length === 0
  );
  await SpecialPowers.popPrefEnv();
  await rsClient.db.clear();
  sandbox.restore();
});
