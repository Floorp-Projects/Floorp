"use strict";

const {
  ExperimentAPI,
  NimbusFeatures,
  _ExperimentFeature: ExperimentFeature,
} = ChromeUtils.import("resource://nimbus/ExperimentAPI.jsm");
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

Cu.importGlobalProperties(["fetch"]);

XPCOMUtils.defineLazyGetter(this, "fetchSchema", async () => {
  const response = await fetch(
    "resource://testing-common/ExperimentFeatureRemote.schema.json"
  );
  const schema = await response.json();
  if (!schema) {
    throw new Error("Failed to load ExperimentFeatureRemote schema");
  }
  return schema.definitions.RemoteFeatureConfigurations;
});

const REMOTE_CONFIGURATION = Object.freeze({
  id: "aboutwelcome",
  configurations: [
    {
      slug: "non-matching-configuration",
      description: "This configuration does not match because of targeting.",
      variables: { skipFocus: false, enabled: false },
      targeting: "false",
      bucketConfig: {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      },
    },
    {
      slug: "matching-configuration",
      description: "This configuration will match targeting.",
      variables: { skipFocus: true, enabled: true },
      targeting: "true",
      bucketConfig: {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      },
    },
  ],
});

const AW_FAKE_MANIFEST = {
  description: "Different manifest with a special test variable",
  isEarlyStartup: true,
  variables: {
    remoteValue: {
      type: "boolean",
      description: "Test value",
    },
    mochitest: {
      type: "boolean",
    },
    enabled: {
      type: "boolean",
    },
    skipFocus: {
      type: "boolean",
    },
  },
};

async function setupForExperimentFeature() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  return { sandbox, manager };
}

add_task(async function validSchema() {
  const ajv = new Ajv({ allErrors: true });
  const validate = ajv.compile(await fetchSchema);

  Assert.ok(
    validate(REMOTE_CONFIGURATION),
    JSON.stringify(validate.errors, null, 2)
  );
});

add_task(async function readyCallAfterStore_with_remote_value() {
  let { sandbox, manager } = await setupForExperimentFeature();
  let feature = new ExperimentFeature("aboutwelcome");

  Assert.ok(feature.getVariable("skipFocus"), "Feature is true by default");

  manager.store.updateRemoteConfigs(
    "aboutwelcome",
    REMOTE_CONFIGURATION.configurations[0]
  );

  // Should resolve because the store will return a dummy remote value
  await feature.ready();

  Assert.ok(!feature.getVariable("skipFocus"), "Loads value from store");
  manager.store._deleteForTests("aboutwelcome");
  sandbox.restore();
});

add_task(async function has_sync_value_before_ready() {
  let { manager } = await setupForExperimentFeature();
  let feature = new ExperimentFeature("aboutwelcome", AW_FAKE_MANIFEST);

  Assert.equal(
    feature.getVariable("remoteValue"),
    undefined,
    "Feature is true by default"
  );

  Services.prefs.setStringPref(
    "nimbus.syncdefaultsstore.aboutwelcome",
    JSON.stringify(REMOTE_CONFIGURATION.configurations[0])
  );

  Services.prefs.setBoolPref(
    "nimbus.syncdefaultsstore.aboutwelcome.remoteValue",
    true
  );

  Assert.equal(feature.getVariable("remoteValue"), true, "Sync load from pref");

  manager.store._deleteForTests("aboutwelcome");
});

add_task(async function update_remote_defaults_onUpdate() {
  let { sandbox, manager } = await setupForExperimentFeature();
  let feature = new ExperimentFeature("aboutwelcome");
  let stub = sandbox.stub();

  feature.onUpdate(stub);

  manager.store.updateRemoteConfigs(
    "aboutwelcome",
    REMOTE_CONFIGURATION.configurations[0]
  );

  Assert.ok(stub.called, "update event called");
  Assert.equal(stub.callCount, 1, "Called once for remote configs");
  Assert.equal(
    stub.firstCall.args[1],
    "remote-defaults-update",
    "Correct reason"
  );

  manager.store._deleteForTests("aboutwelcome");
  sandbox.restore();
});

add_task(async function update_remote_defaults_readyPromise() {
  let { sandbox, manager } = await setupForExperimentFeature();
  let feature = new ExperimentFeature("aboutwelcome");
  let stub = sandbox.stub();
  let promise = feature.ready();

  feature.onUpdate(stub);

  manager.store.updateRemoteConfigs(
    "aboutwelcome",
    REMOTE_CONFIGURATION.configurations[1]
  );

  await promise;

  manager.store._deleteForTests("aboutwelcome");
  sandbox.restore();
});

add_task(async function update_remote_defaults_enabled() {
  let { sandbox, manager } = await setupForExperimentFeature();
  let feature = new ExperimentFeature("aboutwelcome");
  let promise = feature.ready();

  Assert.equal(
    feature.isEnabled(),
    true,
    "Feature is enabled by manifest.variables.enabled"
  );

  manager.store.updateRemoteConfigs(
    "aboutwelcome",
    REMOTE_CONFIGURATION.configurations[0]
  );

  await promise;

  Assert.ok(
    !feature.isEnabled(),
    "Feature is disabled by remote configuration"
  );

  manager.store._deleteForTests("aboutwelcome");
  sandbox.restore();
});

// If the branch data returned from the store is not modified
// this test should not throw
add_task(async function test_getVariable_no_mutation() {
  let { sandbox, manager } = await setupForExperimentFeature();
  sandbox.stub(manager.store, "getExperimentForFeature").returns(
    Cu.cloneInto(
      {
        branch: {
          features: [{ featureId: "aboutwelcome", value: { mochitest: true } }],
        },
      },
      {},
      { deepFreeze: true }
    )
  );
  let feature = new ExperimentFeature("aboutwelcome", AW_FAKE_MANIFEST);

  Assert.ok(feature.getVariable("mochitest"), "Got back the expected feature");

  sandbox.restore();
});

add_task(async function remote_isEarlyStartup_config() {
  let { manager } = await setupForExperimentFeature();
  let feature = new ExperimentFeature("password-autocomplete");

  manager.store.updateRemoteConfigs("password-autocomplete", {
    slug: "remote-config-isEarlyStartup",
    description: "This feature normally is not marked isEarlyStartup",
    variables: { remote: true },
    isEarlyStartup: true,
  });

  await feature.ready();

  Assert.ok(
    Services.prefs.prefHasUserValue(
      "nimbus.syncdefaultsstore.password-autocomplete"
    ),
    "Configuration is marked early startup"
  );

  Services.prefs.clearUserPref(
    "nimbus.syncdefaultsstore.password-autocomplete"
  );
});
