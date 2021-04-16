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
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);

const REMOTE_CONFIGURATION_AW = {
  id: "aboutwelcome",
  description: "about:welcome",
  configurations: [
    {
      slug: "a",
      variables: { remoteValue: 24 },
      enabled: false,
      targeting: "false",
    },
    {
      slug: "b",
      variables: { remoteValue: 42 },
      enabled: true,
      targeting: "true",
    },
  ],
};
const REMOTE_CONFIGURATION_NEWTAB = {
  id: "newtab",
  description: "about:newtab",
  configurations: [
    {
      slug: "a",
      variables: { remoteValue: 1 },
      enabled: false,
      targeting: "false",
    },
    {
      slug: "b",
      variables: { remoteValue: 3 },
      enabled: true,
      targeting: "true",
    },
    {
      slug: "c",
      variables: { remoteValue: 2 },
      enabled: false,
      targeting: "false",
    },
  ],
};
const SYNC_DEFAULTS_PREF_BRANCH = "nimbus.syncdefaultsstore.";

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

  return client;
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
  const sandbox = sinon.createSandbox();
  const featureInstance = NimbusFeatures.aboutwelcome;
  const newtabFeature = NimbusFeatures.newtab;
  let stub = sandbox.stub();
  let spy = sandbox.spy(ExperimentAPI._store, "finalizeRemoteConfigs");
  const setExperimentActiveStub = sandbox.stub(
    TelemetryEnvironment,
    "setExperimentActive"
  );
  const setExperimentInactiveStub = sandbox.stub(
    TelemetryEnvironment,
    "setExperimentInactive"
  );

  featureInstance.onUpdate(stub);

  Assert.equal(
    featureInstance.getValue().remoteValue,
    undefined,
    "This prop does not exist before we sync"
  );

  let rsClient = await setup();

  let callCount = spy.callCount;
  await RemoteDefaultsLoader.syncRemoteDefaults();

  Assert.ok(
    spy.callCount === callCount + 1,
    "Called finalize after processing remote configs"
  );

  // We need to await here because remote configurations are processed
  // async to evaluate targeting
  await Promise.all([featureInstance.ready(), newtabFeature.ready()]);

  Assert.ok(featureInstance.isEnabled(), "Enabled by remote defaults");
  Assert.equal(
    featureInstance.getValue().remoteValue,
    REMOTE_CONFIGURATION_AW.configurations[1].variables.remoteValue,
    "aboutwelcome is set by remote defaults"
  );
  Assert.equal(
    newtabFeature.getValue().remoteValue,
    REMOTE_CONFIGURATION_NEWTAB.configurations[1].variables.remoteValue,
    "newtab is set by remote defaults"
  );

  Assert.equal(stub.callCount, 1, "Called by RS sync");
  Assert.equal(
    stub.firstCall.args[1],
    "remote-defaults-update",
    "We receive events on remote defaults updates"
  );

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}newtab`),
    "Pref cache is set"
  );

  // Check if we sent active experiment data for defaults
  Assert.equal(
    setExperimentActiveStub.callCount,
    2,
    "setExperimentActive called once per feature"
  );

  Assert.ok(
    setExperimentActiveStub.calledWith(
      "default-aboutwelcome",
      REMOTE_CONFIGURATION_AW.configurations[1].slug,
      {
        type: "nimbus-default",
        enrollmentId: "__NO_ENROLLMENT_ID__",
      }
    ),
    "should call setExperimentActive with aboutwelcome"
  );
  Assert.ok(
    setExperimentActiveStub.calledWith(
      "default-newtab",
      REMOTE_CONFIGURATION_NEWTAB.configurations[1].slug,
      {
        type: "nimbus-default",
        enrollmentId: "__NO_ENROLLMENT_ID__",
      }
    ),
    "should call setExperimentActive with newtab"
  );

  // Clear RS db and load again. No configurations so should clear the cache.
  await rsClient.db.clear();
  await RemoteDefaultsLoader.syncRemoteDefaults();

  Assert.ok(
    spy.callCount === callCount + 2,
    "Called a second time by syncRemoteDefaults"
  );

  Assert.ok(stub.calledTwice, "Second update is from the removal");
  Assert.equal(
    stub.secondCall.args[1],
    "remote-defaults-update",
    "We receive events when the remote configuration is removed"
  );

  // Check if we sent active experiment data for defaults
  Assert.equal(
    setExperimentInactiveStub.callCount,
    2,
    "setExperimentInactive called once per feature"
  );

  Assert.ok(
    setExperimentInactiveStub.calledWith("default-aboutwelcome"),
    "should call setExperimentInactive with aboutwelcome"
  );
  Assert.ok(
    setExperimentInactiveStub.calledWith("default-newtab"),
    "should call setExperimentInactive with aboutnewtab"
  );

  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}newtab`, ""),
    "Should clear the pref"
  );
  Assert.ok(!newtabFeature.getValue().remoteValue, "Should be missing");

  featureInstance.off(stub);
  ExperimentAPI._store._deleteForTests("aboutwelcome");
  ExperimentAPI._store._deleteForTests("newtab");
  // The Promise for remote defaults has been resolved so we need
  // clean state for the next run
  NimbusFeatures.aboutwelcome = new ExperimentFeature("aboutwelcome");
  NimbusFeatures.newtab = new ExperimentFeature("newtab");
  sandbox.restore();
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

// Test that awaiting `feature.ready()` resolves even when there is no remote
// data
add_task(async function test_remote_fetch_no_data_syncRemoteBefore() {
  const featureInstance = NimbusFeatures.aboutwelcome;
  const featureFoo = new ExperimentFeature("foo", {
    foo: { description: "mochitests" },
  });
  const stub = sinon.stub();

  ExperimentAPI._store.on("remote-defaults-finalized", stub);

  await setup();

  await RemoteDefaultsLoader.syncRemoteDefaults();

  // featureFoo will also resolve when the remote defaults cycle finishes
  await Promise.all([featureInstance.ready(), featureFoo.ready()]);

  Assert.equal(stub.callCount, 1, "Notified all features");

  ExperimentAPI._store.off("remote-defaults-finalized", stub);
  ExperimentAPI._store._deleteForTests("aboutwelcome");
  ExperimentAPI._store._deleteForTests("foo");
  // The Promise for remote defaults has been resolved so we need
  // clean state for the next run
  NimbusFeatures.aboutwelcome = new ExperimentFeature("aboutwelcome");
  NimbusFeatures.newtab = new ExperimentFeature("newtab");
});

// Test that awaiting `feature.ready()` resolves even when there is no remote
// data
add_task(async function test_remote_fetch_no_data_noWaitRemoteLoad() {
  const featureInstance = NimbusFeatures.aboutwelcome;
  const featureFoo = new ExperimentFeature("foo", {
    foo: { description: "mochitests" },
  });
  const stub = sinon.stub();

  ExperimentAPI._store.on("remote-defaults-finalized", stub);

  await setup();

  // Don't wait to load remote defaults; make sure there is no blocking issue
  // with the `ready` call
  RemoteDefaultsLoader.syncRemoteDefaults();

  // featureFoo will also resolve when the remote defaults cycle finishes
  await Promise.all([featureInstance.ready(), featureFoo.ready()]);

  Assert.equal(stub.callCount, 1, "Notified all features");

  ExperimentAPI._store.off("remote-defaults-finalized", stub);
  ExperimentAPI._store._deleteForTests("aboutwelcome");
  ExperimentAPI._store._deleteForTests("foo");
  // The Promise for remote defaults has been resolved so we need
  // clean state for the next run
  NimbusFeatures.aboutwelcome = new ExperimentFeature("aboutwelcome");
  NimbusFeatures.newtab = new ExperimentFeature("newtab");
});

add_task(async function test_remote_ready_from_experiment() {
  const featureFoo = new ExperimentFeature("foo", {
    foo: { description: "mochitests" },
  });

  await ExperimentAPI._store.ready();

  let {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(
    ExperimentFakes.recipe("mochitest-1-foo", {
      branches: [
        {
          slug: "mochitest-1-foo",
          feature: {
            enabled: true,
            featureId: "foo",
            value: null,
          },
        },
      ],
      active: true,
    })
  );

  await enrollmentPromise;
  // featureFoo will also resolve when the remote defaults cycle finishes
  await featureFoo.ready();

  Assert.ok(
    true,
    "We resolved the remote defaults ready by enrolling in an experiment that targets this feature"
  );

  await doExperimentCleanup();
});

add_task(async function test_finalizeRemoteConfigs_cleanup() {
  const SYNC_DEFAULTS_PREF_BRANCH = "nimbus.syncdefaultsstore.";
  const featureFoo = new ExperimentFeature("foo", {
    foo: { description: "mochitests" },
  });
  const featureBar = new ExperimentFeature("bar", {
    foo: { description: "mochitests" },
  });
  let stubFoo = sinon.stub();
  let stubBar = sinon.stub();
  featureFoo.onUpdate(stubFoo);
  featureBar.onUpdate(stubBar);

  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}foo`,
    JSON.stringify({ foo: true })
  );
  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}bar`,
    JSON.stringify({ bar: true })
  );

  ExperimentAPI._store.finalizeRemoteConfigs(["foo"]);

  Assert.ok(stubFoo.notCalled, "Not called, feature seen in session");
  Assert.ok(stubBar.called, "Called, feature not seen in session");
  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}foo`),
    "Pref is not cleared"
  );
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`, ""),
    "Pref was cleared"
  );
  // cleanup
  Services.prefs.clearUserPref(`${SYNC_DEFAULTS_PREF_BRANCH}foo`);
});

add_task(async function remote_defaults_resolve_telemetry_off() {
  await SpecialPowers.pushPrefEnv({
    set: [["app.shield.optoutstudies.enabled", false]],
  });
  let stub = sinon.stub();
  ExperimentAPI._store.on("remote-defaults-finalized", stub);

  const feature = new ExperimentFeature("foo", {
    foo: { description: "test" },
  });
  let promise = feature.ready();

  RemoteSettingsExperimentLoader.init();

  await promise;

  Assert.equal(stub.callCount, 1, "init returns early and resolves the await");
});

add_task(async function remote_defaults_resolve_timeout() {
  const feature = new ExperimentFeature("foo", {
    foo: { description: "test" },
  });

  await feature.ready(0);

  Assert.ok(true, "Resolves waitForRemote");
});
