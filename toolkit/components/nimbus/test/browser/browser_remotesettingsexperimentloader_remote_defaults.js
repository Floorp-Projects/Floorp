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
const { ExperimentTestUtils } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
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
      type: "int",
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
      type: "int",
    },
    enabled: {
      type: "boolean",
    },
  },
};

const ENSURE_ENROLLMENT = {
  targeting: "true",
  bucketConfig: {
    namespace: "nimbus-test-utils",
    randomizationUnit: "normandy_id",
    start: 0,
    count: 1000,
    total: 1000,
  },
};

const REMOTE_CONFIGURATION_FOO = ExperimentFakes.recipe("foo-rollout", {
  isRollout: true,
  branches: [
    {
      slug: "foo-rollout-branch",
      ratio: 1,
      features: [
        {
          featureId: "foo",
          enabled: true,
          isEarlyStartup: true,
          value: { remoteValue: 42, enabled: true },
        },
      ],
    },
  ],
  ...ENSURE_ENROLLMENT,
});
const REMOTE_CONFIGURATION_BAR = ExperimentFakes.recipe("bar-rollout", {
  isRollout: true,
  branches: [
    {
      slug: "bar-rollout-branch",
      ratio: 1,
      features: [
        {
          featureId: "bar",
          enabled: true,
          isEarlyStartup: true,
          value: { remoteValue: 3, enabled: true },
        },
      ],
    },
  ],
  ...ENSURE_ENROLLMENT,
});

const SYNC_DEFAULTS_PREF_BRANCH = "nimbus.syncdefaultsstore.";

async function setup(configuration) {
  const client = RemoteSettings("nimbus-desktop-experiments");
  await client.db.importChanges(
    {},
    Date.now(),
    configuration || [REMOTE_CONFIGURATION_FOO, REMOTE_CONFIGURATION_BAR],
    {
      clear: true,
    }
  );

  const cleanup = () => client.db.clear();
  return { client, cleanup };
}

add_task(async function test_remote_fetch_and_ready() {
  const fooInstance = new ExperimentFeature("foo", FOO_FAKE_FEATURE_MANIFEST);
  const barInstance = new ExperimentFeature("bar", BAR_FAKE_FEATURE_MANIFEST);

  const cleanupTestFeatures = ExperimentTestUtils.addTestFeatures(
    fooInstance,
    barInstance
  );

  const sandbox = sinon.createSandbox();
  const setExperimentActiveStub = sandbox.stub(
    TelemetryEnvironment,
    "setExperimentActive"
  );
  const setExperimentInactiveStub = sandbox.stub(
    TelemetryEnvironment,
    "setExperimentInactive"
  );

  Assert.equal(
    fooInstance.getVariable("remoteValue"),
    undefined,
    "This prop does not exist before we sync"
  );

  // Create to promises that get resolved when the features update
  // with the remote setting rollouts
  let fooUpdate = new Promise(resolve => fooInstance.onUpdate(resolve));
  let barUpdate = new Promise(resolve => barInstance.onUpdate(resolve));

  await ExperimentAPI.ready();

  let { client: rsClient, cleanup } = await setup();

  // Fake being initialized so we can update recipes
  // we don't need to start any timers
  RemoteSettingsExperimentLoader._initialized = true;
  await RemoteSettingsExperimentLoader.updateRecipes(
    "browser_rsel_remote_defaults"
  );

  // We need to await here because remote configurations are processed
  // async to evaluate targeting
  await Promise.all([fooUpdate, barUpdate]);

  Assert.ok(fooInstance.isEnabled(), "Enabled by remote defaults");
  Assert.equal(
    fooInstance.getVariable("remoteValue"),
    REMOTE_CONFIGURATION_FOO.branches[0].features[0].value.remoteValue,
    "`foo` feature is set by remote defaults"
  );
  Assert.equal(
    barInstance.getVariable("remoteValue"),
    REMOTE_CONFIGURATION_BAR.branches[0].features[0].value.remoteValue,
    "`bar` feature is set by remote defaults"
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
      REMOTE_CONFIGURATION_FOO.slug,
      REMOTE_CONFIGURATION_FOO.branches[0].slug,
      {
        type: "nimbus-rollout",
        enrollmentId: sinon.match.string,
      }
    ),
    "should call setExperimentActive with `foo` feature"
  );
  Assert.ok(
    setExperimentActiveStub.calledWith(
      REMOTE_CONFIGURATION_BAR.slug,
      REMOTE_CONFIGURATION_BAR.branches[0].slug,
      {
        type: "nimbus-rollout",
        enrollmentId: sinon.match.string,
      }
    ),
    "should call setExperimentActive with `bar` feature"
  );

  // Test Glean experiment API interaction
  Assert.equal(
    Services.fog.testGetExperimentData(REMOTE_CONFIGURATION_FOO.slug).branch,
    REMOTE_CONFIGURATION_FOO.branches[0].slug,
    "Glean.setExperimentActive called with `foo` feature"
  );
  Assert.equal(
    Services.fog.testGetExperimentData(REMOTE_CONFIGURATION_BAR.slug).branch,
    REMOTE_CONFIGURATION_BAR.branches[0].slug,
    "Glean.setExperimentActive called with `bar` feature"
  );

  Assert.equal(fooInstance.getVariable("remoteValue"), 42, "Has rollout value");
  Assert.equal(barInstance.getVariable("remoteValue"), 3, "Has rollout value");

  // Clear RS db and load again. No configurations so should clear the cache.
  await rsClient.db.clear();
  await RemoteSettingsExperimentLoader.updateRecipes(
    "browser_rsel_remote_defaults"
  );

  Assert.ok(
    !fooInstance.getVariable("remoteValue"),
    "foo-rollout should be removed"
  );
  Assert.ok(
    !barInstance.getVariable("remoteValue"),
    "bar-rollout should be removed"
  );

  // Check if we sent active experiment data for defaults
  Assert.equal(
    setExperimentInactiveStub.callCount,
    2,
    "setExperimentInactive called once per feature"
  );

  Assert.ok(
    setExperimentInactiveStub.calledWith(REMOTE_CONFIGURATION_FOO.slug),
    "should call setExperimentInactive with `foo` feature"
  );
  Assert.ok(
    setExperimentInactiveStub.calledWith(REMOTE_CONFIGURATION_BAR.slug),
    "should call setExperimentInactive with `bar` feature"
  );

  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`, ""),
    "Should clear the pref"
  );
  Assert.ok(!barInstance.getVariable("remoteValue"), "Should be missing");

  ExperimentAPI._store._deleteForTests("foo");
  ExperimentAPI._store._deleteForTests("bar");
  ExperimentAPI._store._deleteForTests(REMOTE_CONFIGURATION_FOO.slug);
  ExperimentAPI._store._deleteForTests(REMOTE_CONFIGURATION_BAR.slug);
  sandbox.restore();

  cleanupTestFeatures();
  await cleanup();
});

add_task(async function test_remote_fetch_on_updateRecipes() {
  let sandbox = sinon.createSandbox();
  let updateRecipesStub = sandbox.stub(
    RemoteSettingsExperimentLoader,
    "updateRecipes"
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
    () => updateRecipesStub.called,
    "Wait for timer to call"
  );

  Assert.ok(updateRecipesStub.calledOnce, "Timer calls function");
  Assert.equal(updateRecipesStub.firstCall.args[0], "timer", "Called by timer");
  sandbox.restore();
  // This will un-register the timer
  RemoteSettingsExperimentLoader._initialized = true;
  RemoteSettingsExperimentLoader.uninit();
  Services.prefs.clearUserPref(
    "app.update.lastUpdateTime.rs-experiment-loader-timer"
  );
});

add_task(async function test_finalizeRemoteConfigs_cleanup() {
  const featureFoo = new ExperimentFeature("foo", {
    description: "mochitests",
    variables: {
      foo: { type: "boolean" },
    },
  });
  const featureBar = new ExperimentFeature("bar", {
    description: "mochitests",
    variables: {
      bar: { type: "boolean" },
    },
  });

  const cleanupTestFeatures = ExperimentTestUtils.addTestFeatures(
    featureFoo,
    featureBar
  );

  let fooCleanup = await ExperimentFakes.enrollWithRollout(
    {
      featureId: "foo",
      enabled: true,
      isEarlyStartup: true,
      value: { foo: true },
    },
    {
      source: "rs-loader",
    }
  );
  await ExperimentFakes.enrollWithRollout(
    {
      featureId: "bar",
      enabled: true,
      isEarlyStartup: true,
      value: { bar: true },
    },
    {
      source: "rs-loader",
    }
  );
  let stubFoo = sinon.stub();
  let stubBar = sinon.stub();
  featureFoo.onUpdate(stubFoo);
  featureBar.onUpdate(stubBar);
  let cleanupPromise = new Promise(resolve => featureBar.onUpdate(resolve));

  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}foo`,
    JSON.stringify({ foo: true, branch: { feature: { featureId: "foo" } } })
  );
  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}bar`,
    JSON.stringify({ bar: true, branch: { feature: { featureId: "bar" } } })
  );

  const remoteConfiguration = {
    ...REMOTE_CONFIGURATION_FOO,
    branches: [
      {
        ...REMOTE_CONFIGURATION_FOO.branches[0],
        features: [
          {
            ...REMOTE_CONFIGURATION_FOO.branches[0].features[0],
            value: {
              foo: true,
            },
          },
        ],
      },
    ],
  };

  const { cleanup } = await setup([remoteConfiguration]);
  RemoteSettingsExperimentLoader._initialized = true;
  await RemoteSettingsExperimentLoader.updateRecipes();
  await cleanupPromise;

  Assert.ok(
    stubFoo.notCalled,
    "Not called, not enrolling in rollout feature already exists"
  );
  Assert.ok(stubBar.called, "Called because no recipe is seen, cleanup");
  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}foo`),
    "Pref is not cleared"
  );
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`, ""),
    "Pref was cleared"
  );

  await fooCleanup();
  // This will also remove the inactive recipe from the store
  // the previous update (from recipe not seen code path)
  // only sets the recipe as inactive
  ExperimentAPI._store._deleteForTests("bar-rollout");
  ExperimentAPI._store._deleteForTests("foo-rollout");

  cleanupTestFeatures();
  cleanup();
});

// If the remote config data returned from the store is not modified
// this test should not throw
add_task(async function remote_defaults_no_mutation() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(ExperimentAPI._store, "getRolloutForFeature").returns(
    Cu.cloneInto(
      {
        featureIds: ["foo"],
        branch: {
          features: [{ featureId: "foo", value: { remoteStub: true } }],
        },
      },
      {},
      { deepFreeze: true }
    )
  );

  let fooInstance = new ExperimentFeature("foo", FOO_FAKE_FEATURE_MANIFEST);
  let config = fooInstance.getAllVariables();

  Assert.ok(config.remoteStub, "Got back the expected value");

  sandbox.restore();
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

  const cleanupTestFeatures = ExperimentTestUtils.addTestFeatures(
    barFeature,
    fooFeature
  );

  let rollout1 = ExperimentFakes.recipe("bar", {
    branches: [
      {
        slug: "bar-rollout-branch",
        ratio: 1,
        features: [
          {
            featureId: "bar",
            value: { enabled: true },
          },
        ],
      },
    ],
    isRollout: true,
    ...ENSURE_ENROLLMENT,
    targeting: "true",
  });

  let rollout2 = ExperimentFakes.recipe("foo", {
    branches: [
      {
        slug: "foo-rollout-branch",
        ratio: 1,
        features: [
          {
            featureId: "foo",
            value: { enabled: true },
          },
        ],
      },
    ],
    isRollout: true,
    ...ENSURE_ENROLLMENT,
    targeting: "'bar' in activeRollouts",
  });

  // Order is important, rollout2 won't match at first
  const { cleanup } = await setup([rollout2, rollout1]);
  let updatePromise = new Promise(resolve => barFeature.onUpdate(resolve));
  RemoteSettingsExperimentLoader._initialized = true;
  await RemoteSettingsExperimentLoader.updateRecipes("mochitest");

  await updatePromise;

  Assert.ok(barFeature.isEnabled(), "Enabled on first sync");
  Assert.ok(!fooFeature.isEnabled(), "Targeting doesn't match");

  let featureUpdate = new Promise(resolve => fooFeature.onUpdate(resolve));
  await RemoteSettingsExperimentLoader.updateRecipes("mochitest");
  await featureUpdate;

  Assert.ok(fooFeature.isEnabled(), "Targeting should match");
  ExperimentAPI._store._deleteForTests("foo");
  ExperimentAPI._store._deleteForTests("bar");

  cleanup();
  cleanupTestFeatures();
});

add_task(async function remote_defaults_variables_storage() {
  let barFeature = new ExperimentFeature("bar", {
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
  });
  let rolloutValue = {
    storage: 42,
    object: { foo: "foo" },
    string: "string",
    bool: true,
    enabled: true,
  };

  let doCleanup = await ExperimentFakes.enrollWithRollout({
    featureId: "bar",
    enabled: true,
    isEarlyStartup: true,
    value: rolloutValue,
  });

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
    barFeature.getAllVariables(),
    rolloutValue,
    "Test types are returned correctly"
  );

  await doCleanup();

  Assert.equal(
    Services.prefs.getIntPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar.storage`, -1),
    -1,
    "Variable pref is cleared"
  );
  Assert.ok(!barFeature.getVariable("string"), "Variable is no longer defined");
  ExperimentAPI._store._deleteForTests("bar");
  ExperimentAPI._store._deleteForTests("bar-rollout");
});
