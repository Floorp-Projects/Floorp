/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const {
  _ExperimentFeature: ExperimentFeature,
  NimbusFeatures,
  ExperimentAPI,
} = ChromeUtils.import("resource://nimbus/ExperimentAPI.jsm");
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
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
const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);

const FOO_FAKE_FEATURE_MANIFEST = {
  isEarlyStartup: true,
  variables: {
    remoteValue: {
      type: "boolean",
    },
    enabled: {
      type: "boolean",
    },
  },
};

const BAR_FAKE_FEATURE_MANIFEST = {
  isEarlyStartup: true,
  variables: {
    remoteValue: {
      type: "boolean",
    },
    enabled: {
      type: "boolean",
    },
  },
};

const REMOTE_CONFIGURATION_FOO = {
  id: "foo",
  description: "foo remote feature value",
  configurations: [
    {
      slug: "a",
      variables: { remoteValue: 24, enabled: false },
      targeting: "false",
      isEarlyStartup: true,
      bucketConfig: {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      },
    },
    {
      slug: "b",
      variables: { remoteValue: 42, enabled: true },
      targeting: "true",
      isEarlyStartup: true,
      bucketConfig: {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      },
    },
  ],
};
const REMOTE_CONFIGURATION_BAR = {
  id: "bar",
  description: "bar remote feature value",
  configurations: [
    {
      slug: "a",
      variables: { remoteValue: 1, enabled: false },
      targeting: "false",
      isEarlyStartup: true,
      bucketConfig: {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      },
    },
    {
      slug: "b",
      variables: { remoteValue: 3, enabled: true },
      targeting: "true",
      isEarlyStartup: true,
      bucketConfig: {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      },
    },
    {
      slug: "c",
      variables: { remoteValue: 2, enabled: false },
      targeting: "false",
      isEarlyStartup: true,
      bucketConfig: {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      },
    },
  ],
};
const SYNC_DEFAULTS_PREF_BRANCH = "nimbus.syncdefaultsstore.";

async function setup(configuration) {
  const client = RemoteSettings("nimbus-desktop-defaults");
  await client.db.importChanges(
    {},
    42,
    configuration || [REMOTE_CONFIGURATION_FOO, REMOTE_CONFIGURATION_BAR],
    {
      clear: true,
    }
  );

  registerCleanupFunction(async () => {
    await client.db.clear();
  });

  return client;
}

add_task(async function test_remote_fetch_and_ready() {
  const sandbox = sinon.createSandbox();
  const fooInstance = new ExperimentFeature("foo", FOO_FAKE_FEATURE_MANIFEST);
  const barInstance = new ExperimentFeature("bar", BAR_FAKE_FEATURE_MANIFEST);
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
  ExperimentAPI._store._deleteForTests("foo");
  ExperimentAPI._store._deleteForTests("bar");

  fooInstance.onUpdate(stub);

  Assert.equal(
    fooInstance.getVariable("remoteValue"),
    undefined,
    "This prop does not exist before we sync"
  );

  let rsClient = await setup();

  await RemoteDefaultsLoader.syncRemoteDefaults();

  Assert.equal(
    spy.callCount,
    1,
    "Called finalize after processing remote configs"
  );

  // We need to await here because remote configurations are processed
  // async to evaluate targeting
  await Promise.all([fooInstance.ready(), barInstance.ready()]);

  Assert.ok(fooInstance.isEnabled(), "Enabled by remote defaults");
  Assert.equal(
    fooInstance.getVariable("remoteValue"),
    REMOTE_CONFIGURATION_FOO.configurations[1].variables.remoteValue,
    "`foo` feature is set by remote defaults"
  );
  Assert.equal(
    barInstance.getVariable("remoteValue"),
    REMOTE_CONFIGURATION_BAR.configurations[1].variables.remoteValue,
    "`bar` feature is set by remote defaults"
  );

  Assert.equal(stub.callCount, 1, "Called by RS sync");
  Assert.equal(
    stub.firstCall.args[1],
    "remote-defaults-update",
    "We receive events on remote defaults updates"
  );

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`),
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
      "default-foo",
      REMOTE_CONFIGURATION_FOO.configurations[1].slug,
      {
        type: "nimbus-default",
        enrollmentId: "__NO_ENROLLMENT_ID__",
      }
    ),
    "should call setExperimentActive with `foo` feature"
  );
  Assert.ok(
    setExperimentActiveStub.calledWith(
      "default-bar",
      REMOTE_CONFIGURATION_BAR.configurations[1].slug,
      {
        type: "nimbus-default",
        enrollmentId: "__NO_ENROLLMENT_ID__",
      }
    ),
    "should call setExperimentActive with `bar` feature"
  );

  // Clear RS db and load again. No configurations so should clear the cache.
  await rsClient.db.clear();
  await RemoteDefaultsLoader.syncRemoteDefaults();

  Assert.equal(spy.callCount, 2, "Called a second time by syncRemoteDefaults");

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
    setExperimentInactiveStub.calledWith("default-foo"),
    "should call setExperimentInactive with `foo` feature"
  );
  Assert.ok(
    setExperimentInactiveStub.calledWith("default-bar"),
    "should call setExperimentInactive with `bar` feature"
  );

  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`, ""),
    "Should clear the pref"
  );
  Assert.ok(!barInstance.getVariable("remoteValue"), "Should be missing");

  fooInstance.off(stub);
  ExperimentAPI._store._deleteForTests("foo");
  ExperimentAPI._store._deleteForTests("bar");
  sandbox.restore();
});

add_task(async function test_remote_fetch_on_updateRecipes() {
  let sandbox = sinon.createSandbox();
  let syncRemoteDefaultsStub = sandbox.stub(
    RemoteDefaultsLoader,
    "syncRemoteDefaults"
  );
  // Work around the pref change callback that would trigger `setTimer`
  sandbox.replaceGetter(
    RemoteSettingsExperimentLoader,
    "intervalInSeconds",
    () => 1
  );

  // This will un-register the timer
  RemoteSettingsExperimentLoader._initialized = true;
  RemoteSettingsExperimentLoader.uninit();
  Services.prefs.clearUserPref(
    "app.update.lastUpdateTime.rs-experiment-loader-timer"
  );

  RemoteSettingsExperimentLoader.setTimer();

  await BrowserTestUtils.waitForCondition(
    () => syncRemoteDefaultsStub.called,
    "Wait for timer to call"
  );

  Assert.ok(syncRemoteDefaultsStub.calledOnce, "Timer calls function");
  Assert.equal(
    syncRemoteDefaultsStub.firstCall.args[0],
    "timer",
    "Called by timer"
  );
  sandbox.restore();
  // This will un-register the timer
  RemoteSettingsExperimentLoader._initialized = true;
  RemoteSettingsExperimentLoader.uninit();
  Services.prefs.clearUserPref(
    "app.update.lastUpdateTime.rs-experiment-loader-timer"
  );
});

// Test that awaiting `feature.ready()` resolves even when there is no remote
// data
add_task(async function test_remote_fetch_no_data_syncRemoteBefore() {
  const sandbox = sinon.createSandbox();
  const fooInstance = new ExperimentFeature("foo", FOO_FAKE_FEATURE_MANIFEST);
  const barInstance = new ExperimentFeature("bar", {
    bar: { description: "mochitests" },
  });
  const stub = sandbox.stub();
  const spy = sandbox.spy(ExperimentAPI._store, "finalizeRemoteConfigs");

  ExperimentAPI._store.on("remote-defaults-finalized", stub);

  await setup();

  await RemoteDefaultsLoader.syncRemoteDefaults();

  // featureFoo will also resolve when the remote defaults cycle finishes
  await Promise.all([fooInstance.ready(), barInstance.ready()]);

  Assert.ok(spy.calledOnce, "Called finalizeRemoteConfigs");
  Assert.deepEqual(spy.firstCall.args[0], ["bar", "foo"]);
  Assert.equal(stub.callCount, 1, "Notified all features");

  ExperimentAPI._store.off("remote-defaults-finalized", stub);
  ExperimentAPI._store._deleteForTests("foo");
  ExperimentAPI._store._deleteForTests("bar");
  sandbox.restore();
});

// Test that awaiting `feature.ready()` resolves even when there is no remote
// data
add_task(async function test_remote_fetch_no_data_noWaitRemoteLoad() {
  const fooInstance = new ExperimentFeature("foo", FOO_FAKE_FEATURE_MANIFEST);
  const barInstance = new ExperimentFeature("bar", {
    bar: { description: "mochitests" },
  });
  const stub = sinon.stub();

  ExperimentAPI._store.on("remote-defaults-finalized", stub);

  await setup([]);

  // Don't wait to load remote defaults; make sure there is no blocking issue
  // with the `ready` call
  RemoteDefaultsLoader.syncRemoteDefaults();

  // featureFoo will also resolve when the remote defaults cycle finishes
  await Promise.all([fooInstance.ready(), barInstance.ready()]);

  Assert.equal(stub.callCount, 1, "Notified all features");

  ExperimentAPI._store.off("remote-defaults-finalized", stub);
  ExperimentAPI._store._deleteForTests("bar");
  ExperimentAPI._store._deleteForTests("foo");
});

add_task(async function test_remote_ready_from_experiment() {
  const featureFoo = new ExperimentFeature("foo", {
    foo: { description: "mochitests" },
  });

  await ExperimentAPI.ready();

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    enabled: true,
    featureId: "foo",
    value: null,
  });

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

  await feature.ready(1);

  Assert.ok(true, "Resolves waitForRemote");
});

// If the remote config data returned from the store is not modified
// this test should not throw
add_task(async function remote_defaults_no_mutation() {
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(ExperimentAPI._store, "getRemoteConfig")
    .returns(
      Cu.cloneInto(
        { targeting: "true", variables: { remoteStub: true } },
        {},
        { deepFreeze: true }
      )
    );

  let fooInstance = new ExperimentFeature("foo", FOO_FAKE_FEATURE_MANIFEST);
  let config = fooInstance.getAllVariables();

  Assert.ok(config.remoteStub, "Got back the expected value");

  sandbox.restore();
});

add_task(async function remote_defaults_active_experiments_check() {
  let barFeature = new ExperimentFeature("bar", {
    description: "mochitest",
    variables: { enabled: { type: "boolean" } },
  });
  let experimentOnlyRemoteDefault = {
    id: "bar",
    description: "if we're in the foo experiment bar should be off",
    configurations: [
      {
        slug: "a",
        variables: { enabled: false },
        targeting: "'mochitest-active-foo' in activeExperiments",
      },
      {
        slug: "b",
        variables: { enabled: true },
        targeting: "true",
      },
    ],
  };

  await setup([experimentOnlyRemoteDefault]);
  await RemoteDefaultsLoader.syncRemoteDefaults("mochitest");
  await barFeature.ready();

  Assert.ok(barFeature.isEnabled(), "First it's enabled");

  let {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(
    ExperimentFakes.recipe("mochitest-active-foo", {
      branches: [
        {
          slug: "mochitest-active-foo",
          features: [
            {
              enabled: true,
              featureId: "foo",
              value: null,
            },
          ],
        },
      ],
      active: true,
    })
  );

  await enrollmentPromise;
  let featureUpdate = new Promise(resolve => barFeature.onUpdate(resolve));
  await RemoteDefaultsLoader.syncRemoteDefaults("mochitests");
  await featureUpdate;

  Assert.ok(
    !barFeature.isEnabled(),
    "We've enrolled in an experiment which makes us match on the first remote default that disables the feature"
  );

  await doExperimentCleanup();
});

add_task(async function remote_defaults_active_remote_defaults() {
  ExperimentAPI._store._deleteForTests("foo");
  ExperimentAPI._store._deleteForTests("bar");
  let barFeature = new ExperimentFeature("bar", {
    description: "mochitest",
    variables: { enabled: { type: "boolean" } },
  });
  let fooFeature = new ExperimentFeature("foo", {
    description: "mochitest",
    variables: { enabled: { type: "boolean" } },
  });
  let remoteDefaults = [
    {
      id: "bar",
      description: "will enroll first try",
      configurations: [
        {
          slug: "a",
          variables: { enabled: true },
          targeting: "true",
        },
      ],
    },
    {
      id: "foo",
      description: "will enroll second try after bar",
      configurations: [
        {
          slug: "b",
          variables: { enabled: true },
          targeting: "'bar' in activeRemoteDefaults",
        },
      ],
    },
  ];

  await setup(remoteDefaults);
  await RemoteDefaultsLoader.syncRemoteDefaults("mochitest");
  await barFeature.ready();

  Assert.ok(barFeature.isEnabled(), "Enabled on first sync");
  Assert.ok(!fooFeature.isEnabled(), "Targeting doesn't match");

  let featureUpdate = new Promise(resolve => fooFeature.onUpdate(resolve));
  await RemoteDefaultsLoader.syncRemoteDefaults("mochitest");
  await featureUpdate;

  Assert.ok(fooFeature.isEnabled(), "Targeting should match");
  ExperimentAPI._store._deleteForTests("foo");
  ExperimentAPI._store._deleteForTests("bar");
});

add_task(async function test_remote_defaults_bucketConfig() {
  const sandbox = sinon.createSandbox();
  let finalizeRemoteConfigsSpy = sandbox.spy(
    ExperimentAPI._store,
    "finalizeRemoteConfigs"
  );
  let isInBucketAllocationStub = sandbox
    .stub(ExperimentManager, "isInBucketAllocation")
    .resolves(false);
  let evaluateJexlStub = sandbox
    .stub(RemoteSettingsExperimentLoader, "evaluateJexl")
    .resolves(true);
  let rsClient = await setup();

  await RemoteDefaultsLoader.syncRemoteDefaults("mochitest");

  Assert.equal(
    isInBucketAllocationStub.callCount,
    5,
    "Bucket allocation is checked"
  );
  Assert.equal(
    evaluateJexlStub.callCount,
    0,
    "We skip targeting if bucket allocation fails"
  );
  Assert.equal(
    finalizeRemoteConfigsSpy.called,
    true,
    "Finally no configs match"
  );
  Assert.deepEqual(
    finalizeRemoteConfigsSpy.firstCall.args[0],
    [],
    "No configs matched because of bucket allocation"
  );

  sandbox.restore();
  await rsClient.db.clear();
});

add_task(async function test_remote_defaults_no_bucketConfig() {
  const sandbox = sinon.createSandbox();
  const remoteConfigNoBucket = {
    id: "aboutwelcome",
    description: "about:welcome",
    configurations: [
      {
        slug: "a",
        variables: { remoteValue: 24, enabled: false },
        targeting: "false",
      },
      {
        slug: "b",
        variables: { remoteValue: 42, enabled: true },
        targeting: "true",
      },
    ],
  };
  let finalizeRemoteConfigsStub = sandbox.stub(
    ExperimentAPI._store,
    "finalizeRemoteConfigs"
  );
  let isInBucketAllocationStub = sandbox
    .stub(ExperimentManager, "isInBucketAllocation")
    .resolves(false);
  let evaluateJexlStub = sandbox.spy(
    RemoteSettingsExperimentLoader,
    "evaluateJexl"
  );
  let rsClient = await setup([remoteConfigNoBucket]);

  await RemoteDefaultsLoader.syncRemoteDefaults("mochitest");

  Assert.ok(isInBucketAllocationStub.notCalled, "No bucket config to call");
  Assert.equal(evaluateJexlStub.callCount, 2, "Called for two remote configs");
  Assert.deepEqual(
    finalizeRemoteConfigsStub.firstCall.args[0],
    ["aboutwelcome"],
    "Match the config with targeting set to `true`"
  );

  sandbox.restore();
  await rsClient.db.clear();
});

add_task(async function remote_defaults_variables_storage() {
  let barFeature = new ExperimentFeature("bar", {
    bar: {
      description: "mochitest",
      variables: {
        storage: {
          type: "int",
        },
        object: {
          type: "json",
        },
        string: {
          type: "string",
        },
        bool: {
          type: "boolean",
        },
      },
    },
  });
  let remoteDefaults = [
    {
      id: "bar",
      description: "test pref storage and types",
      configurations: [
        {
          slug: "a",
          isEarlyStartup: true,
          variables: {
            storage: 42,
            object: { foo: "foo" },
            string: "string",
            bool: true,
            enabled: true,
          },
          targeting: "true",
        },
      ],
    },
  ];

  await setup(remoteDefaults);
  await RemoteDefaultsLoader.syncRemoteDefaults("mochitest");
  await barFeature.ready();

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`, ""),
    "Experiment stored in prefs"
  );
  Assert.ok(
    Services.prefs.getIntPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar.storage`, 0),
    "Stores variable in separate pref"
  );
  Assert.equal(
    Services.prefs.getIntPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar.storage`, 0),
    42,
    "Stores variable in correct type"
  );
  Assert.deepEqual(
    barFeature.getRemoteConfig().variables,
    remoteDefaults[0].configurations[0].variables,
    "Test types are returned correctly"
  );

  ExperimentAPI._store._deleteForTests("bar");

  Assert.equal(
    Services.prefs.getIntPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar.storage`, -1),
    -1,
    "Variable pref is cleared"
  );
});
