"use strict";

const {
  ExperimentAPI,
  _ExperimentFeature: ExperimentFeature,
} = ChromeUtils.import("resource://nimbus/ExperimentAPI.jsm");
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { cleanupStorePrefCache } = ExperimentFakes;

async function setupForExperimentFeature() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  return { sandbox, manager };
}

const FEATURE_ID = "aboutwelcome";
const TEST_FALLBACK_PREF = "browser.aboutwelcome.screens";
const FAKE_FEATURE_MANIFEST = {
  variables: {
    screens: {
      type: "json",
      fallbackPref: TEST_FALLBACK_PREF,
    },
  },
};

add_task(async function test_ExperimentFeature_getValue_prefsOverDefaults() {
  const { sandbox } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature(
    FEATURE_ID,
    FAKE_FEATURE_MANIFEST
  );

  Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

  Assert.equal(
    featureInstance.getValue().screens?.length,
    undefined,
    "pref is not set"
  );

  Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[]");

  Assert.deepEqual(
    featureInstance.getValue().screens.length,
    0,
    "should return the user pref value over the defaults"
  );

  Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
  sandbox.restore();
});

add_task(async function test_ExperimentFeature_getValue_prefsOverExperiment() {
  const { sandbox, manager } = await setupForExperimentFeature();
  const { doExperimentCleanup } = ExperimentFakes.enrollmentHelper(undefined, {
    manager,
  });
  const recipe = ExperimentFakes.experiment("awexperiment", {
    branch: {
      slug: "treatment",
      feature: {
        featureId: "aboutwelcome",
        enabled: true,
        value: { screens: ["test-value"] },
      },
    },
  });

  manager.store.addExperiment(recipe);

  const featureInstance = new ExperimentFeature(
    FEATURE_ID,
    FAKE_FEATURE_MANIFEST
  );

  Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

  Assert.ok(
    !!featureInstance.getValue().screens,
    "should return the AW experiment value"
  );

  Assert.equal(
    featureInstance.getValue().screens[0],
    "test-value",
    "should return the AW experiment value"
  );

  Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[]");

  Assert.deepEqual(
    featureInstance.getValue().screens.length,
    0,
    "should return the user pref value"
  );

  Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
  await doExperimentCleanup();
  sandbox.restore();
});

add_task(
  async function test_ExperimentFeature_getValue_remoteOverPrefDefaults() {
    const { manager } = await setupForExperimentFeature();
    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.equal(
      featureInstance.getValue().screens?.length,
      undefined,
      "Pref is not set"
    );

    // Load remote defaults
    manager.store.updateRemoteConfigs(FEATURE_ID, {
      variables: { screens: [] },
    });

    // Wait for feature to load remote defaults
    await featureInstance.ready();

    Assert.deepEqual(
      featureInstance.getValue().screens?.length,
      0,
      "Should return the remote value over the defaults"
    );

    Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[1,2,3]");

    Assert.deepEqual(
      featureInstance.getValue().screens.length,
      3,
      "should return the user pref value over the remote"
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
    cleanupStorePrefCache();
  }
);
