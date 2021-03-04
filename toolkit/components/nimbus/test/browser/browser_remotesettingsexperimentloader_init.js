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
const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

function getRecipe(slug) {
  return ExperimentFakes.recipe(slug, {
    bucketConfig: {
      start: 0,
      // Make sure the experiment enrolls
      count: 10000,
      total: 10000,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
    targeting: "!(experiment.slug in activeExperiments)",
  });
}

let rsClient = RemoteSettings("nimbus-desktop-experiments");

add_task(async function setup() {
  RemoteSettingsExperimentLoader._updating = true;
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["app.shield.optoutstudies.enabled", true],
    ],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function test_double_feature_enrollment() {
  await rsClient.db.importChanges(
    {},
    42,
    [getRecipe("foo" + Math.random()), getRecipe("foo" + Math.random())],
    {
      clear: true,
    }
  );
  let { doExperimentCleanup } = ExperimentFakes.enrollmentHelper();
  RemoteSettingsExperimentLoader.uninit();
  await doExperimentCleanup();
  let sandbox = sinon.createSandbox();
  sandbox.stub(RemoteSettingsExperimentLoader, "setTimer");
  sandbox.stub(RemoteSettingsExperimentLoader, "onEnabledPrefChange");
  let sendFailureTelemetryStub = sandbox.stub(
    ExperimentManager,
    "sendFailureTelemetry"
  );
  let enrolledPromise = ExperimentFakes.waitForExperimentUpdate(ExperimentAPI, {
    featureId: "test-feature",
  });

  Assert.ok(ExperimentManager.store.getAllActive().length === 0, "Clean state");

  await RemoteSettingsExperimentLoader.init();

  await enrolledPromise;

  Assert.ok(
    RemoteSettingsExperimentLoader._initialized,
    "It should initialize and process the recipes"
  );

  Assert.equal(
    ExperimentManager.store.getAllActive().length,
    1,
    "1 active experiment"
  );

  await BrowserTestUtils.waitForCondition(
    () => sendFailureTelemetryStub.callCount,
    "Expected to fail one of the recipes"
  );

  await doExperimentCleanup();
  await SpecialPowers.popPrefEnv();
  await rsClient.db.clear();
  sandbox.restore();
});
