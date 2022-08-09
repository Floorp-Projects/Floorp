"use strict";

const {
  ExperimentAPI,
  _ExperimentFeature: ExperimentFeature,
} = ChromeUtils.import("resource://nimbus/ExperimentAPI.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

async function setupForExperimentFeature() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  return { sandbox, manager };
}

const FEATURE_ID = "testfeature1";
// Note: this gets deleted at the end of tests
const TEST_PREF_BRANCH = "testfeature1.";
const TEST_VARIABLES = {
  enabled: {
    type: "boolean",
    fallbackPref: `${TEST_PREF_BRANCH}enabled`,
  },
  name: {
    type: "string",
    fallbackPref: `${TEST_PREF_BRANCH}name`,
  },
  count: {
    type: "int",
    fallbackPref: `${TEST_PREF_BRANCH}count`,
  },
  items: {
    type: "json",
    fallbackPref: `${TEST_PREF_BRANCH}items`,
  },
};

function createInstanceWithVariables(variables) {
  return new ExperimentFeature(FEATURE_ID, {
    variables,
  });
}

add_task(async function test_ExperimentFeature_getPreferenceName() {
  const instance = createInstanceWithVariables(TEST_VARIABLES);

  Assert.equal(
    instance.getPreferenceName("enabled"),
    "testfeature1.enabled",
    "should return the fallback preference name"
  );
});

add_task(async function test_ExperimentFeature_getVariable_notRegistered() {
  const instance = createInstanceWithVariables(TEST_VARIABLES);

  if (Cu.isInAutomation || AppConstants.NIGHTLY_BUILD) {
    Assert.throws(
      () => {
        instance.getVariable("non_existant_variable");
      },
      /Nimbus: Warning - variable "non_existant_variable" is not defined in FeatureManifest\.js/,
      "should throw in automation for variables not defined in the manifest"
    );
  } else {
    info("Won't throw when running in Beta and release candidates");
  }
});

add_task(async function test_ExperimentFeature_getVariable_noFallbackPref() {
  const instance = createInstanceWithVariables({
    foo: { type: "json" },
  });

  Assert.equal(
    instance.getVariable("foo"),
    undefined,
    "should return undefined if no values are set and no fallback pref is defined"
  );
});

add_task(async function test_ExperimentFeature_getVariable_precedence() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const instance = createInstanceWithVariables(TEST_VARIABLES);
  const prefName = TEST_VARIABLES.items.fallbackPref;
  const rollout = ExperimentFakes.rollout(`${FEATURE_ID}-rollout`, {
    branch: {
      slug: "slug",
      features: [
        {
          featureId: FEATURE_ID,
          value: { items: [4, 5, 6] },
        },
      ],
    },
  });

  Services.prefs.clearUserPref(prefName);

  Assert.equal(
    instance.getVariable("items"),
    undefined,
    "should return undefined if the fallback pref is not set"
  );

  // Default pref values
  Services.prefs.setStringPref(prefName, JSON.stringify([1, 2, 3]));

  Assert.deepEqual(
    instance.getVariable("items"),
    [1, 2, 3],
    "should return the default pref value"
  );

  // Remote default values
  await manager.store.addEnrollment(rollout);

  Assert.deepEqual(
    instance.getVariable("items"),
    [4, 5, 6],
    "should return the remote default value over the default pref value"
  );

  // Experiment values
  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: FEATURE_ID,
      value: {
        items: [7, 8, 9],
      },
    },
    { manager }
  );

  Assert.deepEqual(
    instance.getVariable("items"),
    [7, 8, 9],
    "should return the experiment value over the remote value"
  );

  // Cleanup
  Services.prefs.deleteBranch(TEST_PREF_BRANCH);
  await doExperimentCleanup();
  sandbox.restore();
});

add_task(async function test_ExperimentFeature_getVariable_partial_values() {
  const { sandbox, manager } = await setupForExperimentFeature();
  const instance = createInstanceWithVariables(TEST_VARIABLES);
  const rollout = ExperimentFakes.rollout(`${FEATURE_ID}-rollout`, {
    branch: {
      slug: "slug",
      features: [
        {
          featureId: FEATURE_ID,
          value: { name: "abc" },
        },
      ],
    },
  });

  // Set up a pref value for .enabled,
  // a remote value for .name,
  // an experiment value for .items
  Services.prefs.setBoolPref(TEST_VARIABLES.enabled.fallbackPref, true);
  await manager.store.addEnrollment(rollout);
  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: FEATURE_ID,
      value: {},
    },
    { manager }
  );

  Assert.equal(
    instance.getVariable("enabled"),
    true,
    "should skip missing variables from remote defaults"
  );

  Assert.equal(
    instance.getVariable("name"),
    "abc",
    "should skip missing variables from experiments"
  );

  // Cleanup
  Services.prefs.getDefaultBranch("").deleteBranch(TEST_PREF_BRANCH);
  Services.prefs.deleteBranch(TEST_PREF_BRANCH);
  await doExperimentCleanup();
  sandbox.restore();
});
