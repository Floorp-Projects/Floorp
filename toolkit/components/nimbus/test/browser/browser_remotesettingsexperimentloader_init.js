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
    targeting: "true",
  });
}

add_task(async function setup() {
  let rsClient = RemoteSettings("nimbus-desktop-experiments");

  await rsClient.db.importChanges(
    {},
    42,
    [getRecipe("foo" + Math.random()), getRecipe("bar" + Math.random())],
    {
      clear: true,
    }
  );

  registerCleanupFunction(async () => {
    await rsClient.db.clear();
  });
});

async function setup() {
  let sandbox = sinon.createSandbox();
  // We want to prevent this because it would start a recipe
  // update as soon as we turn on the optoutstudies pref
  sandbox.stub(RemoteSettingsExperimentLoader, "setTimer");
  sandbox.stub(RemoteSettingsExperimentLoader, "onEnabledPrefChange");
  RemoteSettingsExperimentLoader._updating = true;

  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["app.shield.optoutstudies.enabled", true],
    ],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    sandbox.restore();
  });
}

add_task(async function test_double_feature_enrollment() {
  let { doExperimentCleanup } = ExperimentFakes.enrollmentHelper();
  await doExperimentCleanup();
  await setup();
  RemoteSettingsExperimentLoader.uninit();
  let sandbox = sinon.createSandbox();
  let sendFailureTelemetryStub = sandbox.stub(
    ExperimentManager,
    "sendFailureTelemetry"
  );

  Assert.ok(ExperimentManager.store.getAllActive().length === 0, "Clean state");

  await RemoteSettingsExperimentLoader.init();

  await ExperimentFakes.waitForExperimentUpdate(ExperimentAPI, {
    featureId: "test-feature",
  });

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
  sandbox.restore();
});
