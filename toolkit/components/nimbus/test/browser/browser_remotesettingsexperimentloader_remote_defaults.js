/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { ExperimentFeature, NimbusFeatures, ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { RemoteDefaultsLoader } = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
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
    set: [["messaging-system.log", "all"]],
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

  // We need to re-run RS loading because we just finished
  // setting up the fake RS data and missed the `ExperimentManager.onStartup`
  // call.
  RemoteDefaultsLoader._initialized = false;
  await RemoteDefaultsLoader.loadRemoteDefaults();

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
  ExperimentAPI._store._deleteForTests("remoteDefaults");
  // The Promise for remote defaults has been resolved so we need
  // clean state for the next run
  NimbusFeatures.aboutwelcome = new ExperimentFeature("aboutwelcome");
  NimbusFeatures.newtab = new ExperimentFeature("newtab");
});
