/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { ExperimentFeature, NimbusFeatures, ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const {
  RemoteDefaultsLoader,
  RemoteSettingsExperimentLoader,
} = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const REMOTE_CONFIGURATION_AW = {
  id: "aboutwelcome",
  description: "about:welcome",
  configurations: [
    { variables: { remoteValue: 24 }, enabled: false, targeting: "false" },
    { variables: { remoteValue: 42 }, enabled: true, targeting: "true" },
  ],
};
const REMOTE_CONFIGURATION_NEWTAB = {
  id: "newtab",
  description: "about:newtab",
  configurations: [
    { variables: { remoteValue: 1 }, enabled: false, targeting: "false" },
    { variables: { remoteValue: 3 }, enabled: true, targeting: "true" },
    { variables: { remoteValue: 2 }, enabled: false, targeting: "false" },
  ],
};

async function setup() {
  const client = RemoteSettings("nimbus-desktop-defaults");
  await client.db.importChanges(
    {},
    42,
    [REMOTE_CONFIGURATION_AW, REMOTE_CONFIGURATION_NEWTAB],
    {
      clear: true,
    }
  );

  await BrowserTestUtils.waitForCondition(
    async () => (await client.get()).length,
    "RS is ready"
  );

  registerCleanupFunction(async () => {
    await client.db.clear();
  });
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["app.normandy.run_interval_seconds", 1],
    ],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function test_remote_fetch_and_ready() {
  const featureInstance = NimbusFeatures.aboutwelcome;
  const newtabFeature = NimbusFeatures.newtab;
  let stub = sinon.stub();

  featureInstance.onUpdate(stub);

  Assert.equal(
    featureInstance.getValue().remoteValue,
    undefined,
    "This prop does not exist before we sync"
  );

  await setup();

  await RemoteDefaultsLoader.syncRemoteDefaults();

  // We need to await here because remote configurations are processed
  // async to evaluate targeting
  await Promise.all([featureInstance.ready(), newtabFeature.ready()]);

  Assert.ok(featureInstance.isEnabled(), "Enabled by remote defaults");
  Assert.equal(
    featureInstance.getValue().remoteValue,
    REMOTE_CONFIGURATION_AW.configurations[1].variables.remoteValue,
    "Set by remote defaults"
  );
  Assert.equal(
    newtabFeature.getValue().remoteValue,
    REMOTE_CONFIGURATION_NEWTAB.configurations[1].variables.remoteValue,
    "Set by remote defaults"
  );

  Assert.equal(stub.callCount, 1, "Called by RS sync");
  Assert.equal(
    stub.firstCall.args[1],
    "remote-defaults-update",
    "We receive events on remote defaults updates"
  );

  featureInstance.off(stub);
  ExperimentAPI._store._deleteForTests("__REMOTE_DEFAULTS");
  // The Promise for remote defaults has been resolved so we need
  // clean state for the next run
  NimbusFeatures.aboutwelcome = new ExperimentFeature("aboutwelcome");
  NimbusFeatures.newtab = new ExperimentFeature("newtab");
});

add_task(async function test_remote_fetch_on_updateRecipes() {
  let sandbox = sinon.createSandbox();
  let syncRemoteDefaultsStub = sandbox.stub(
    RemoteDefaultsLoader,
    "syncRemoteDefaults"
  );
  timerManager.unregisterTimer("rs-experiment-loader-timer");
  Services.prefs.clearUserPref(
    "app.update.lastUpdateTime.rs-experiment-loader-timer"
  );

  Assert.ok(syncRemoteDefaultsStub.notCalled, "Not called");

  RemoteSettingsExperimentLoader.setTimer();

  await BrowserTestUtils.waitForCondition(
    () => syncRemoteDefaultsStub.called,
    "Wait for timer to call"
  );

  Assert.ok(syncRemoteDefaultsStub.calledOnce, "Timer calls function");
  timerManager.unregisterTimer("rs-experiment-loader-timer");
  sandbox.restore();
});
