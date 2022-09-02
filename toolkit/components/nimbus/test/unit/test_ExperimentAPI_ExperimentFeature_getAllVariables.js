"use strict";

const {
  ExperimentAPI,
  _ExperimentFeature: ExperimentFeature,
} = ChromeUtils.import("resource://nimbus/ExperimentAPI.jsm");
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
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
    source: {
      type: "string",
    },
  },
};

add_task(
  async function test_ExperimentFeature_getAllVariables_prefsOverDefaults() {
    const { sandbox } = await setupForExperimentFeature();

    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.equal(
      featureInstance.getAllVariables().screens?.length,
      undefined,
      "pref is not set"
    );

    Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[]");

    Assert.deepEqual(
      featureInstance.getAllVariables().screens.length,
      0,
      "should return the user pref value over the defaults"
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
    sandbox.restore();
  }
);

add_task(
  async function test_ExperimentFeature_getAllVariables_experimentOverPref() {
    const { sandbox, manager } = await setupForExperimentFeature();
    const recipe = ExperimentFakes.experiment("awexperiment", {
      branch: {
        slug: "treatment",
        features: [
          {
            featureId: "aboutwelcome",
            value: { screens: ["test-value"] },
          },
        ],
      },
    });

    await manager.store.addEnrollment(recipe);

    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.ok(
      !!featureInstance.getAllVariables().screens,
      "should return the AW experiment value"
    );

    Assert.equal(
      featureInstance.getAllVariables().screens[0],
      "test-value",
      "should return the AW experiment value"
    );

    Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[]");
    Assert.equal(
      featureInstance.getAllVariables().screens[0],
      "test-value",
      "should return the AW experiment value"
    );

    await ExperimentFakes.cleanupAll([recipe.slug], { manager });
    Assert.deepEqual(
      featureInstance.getAllVariables().screens.length,
      0,
      "should return the user pref value"
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
    sandbox.restore();
  }
);

add_task(
  async function test_ExperimentFeature_getAllVariables_experimentOverRemote() {
    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
    const { manager } = await setupForExperimentFeature();
    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );
    const recipe = ExperimentFakes.experiment("aw-experiment", {
      branch: {
        slug: "treatment",
        features: [
          {
            featureId: FEATURE_ID,
            value: { screens: ["test-value"] },
          },
        ],
      },
    });
    const rollout = ExperimentFakes.rollout("aw-rollout", {
      branch: {
        slug: "treatment",
        features: [
          { featureId: FEATURE_ID, value: { screens: [], source: "rollout" } },
        ],
      },
    });
    // We're using the store in this test we need to wait for it to load
    await manager.store.ready();

    const rolloutPromise = new Promise(resolve =>
      featureInstance.onUpdate((feature, reason) => {
        if (reason === "rollout-updated") {
          resolve();
        }
      })
    );
    const experimentPromise = new Promise(resolve =>
      featureInstance.onUpdate((feature, reason) => {
        if (reason === "experiment-updated") {
          resolve();
        }
      })
    );
    manager.store.addEnrollment(recipe);
    manager.store.addEnrollment(rollout);
    await rolloutPromise;
    await experimentPromise;

    let allVariables = featureInstance.getAllVariables();

    Assert.equal(allVariables.screens.length, 1, "Returns experiment value");
    Assert.ok(!allVariables.source, "Does not include rollout value");

    await ExperimentFakes.cleanupAll([recipe.slug], { manager });
    cleanupStorePrefCache();
  }
);

add_task(
  async function test_ExperimentFeature_getAllVariables_rolloutOverPrefDefaults() {
    const { manager } = await setupForExperimentFeature();
    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );
    const rollout = ExperimentFakes.rollout("foo-aw", {
      branch: {
        slug: "getAllVariables",
        features: [{ featureId: FEATURE_ID, value: { screens: [] } }],
      },
    });
    // We're using the store in this test we need to wait for it to load
    await manager.store.ready();

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.equal(
      featureInstance.getAllVariables().screens?.length,
      undefined,
      "Pref is not set"
    );

    const updatePromise = new Promise(resolve =>
      featureInstance.onUpdate(resolve)
    );
    // Load remote defaults
    manager.store.addEnrollment(rollout);

    // Wait for feature to load the rollout
    await updatePromise;

    Assert.deepEqual(
      featureInstance.getAllVariables().screens?.length,
      0,
      "Should return the rollout value over the defaults"
    );

    Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[1,2,3]");

    Assert.deepEqual(
      featureInstance.getAllVariables().screens.length,
      0,
      "should return the rollout value over the user pref"
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
    cleanupStorePrefCache();
  }
);

add_task(
  async function test_ExperimentFeature_getAllVariables_defaultValuesParam() {
    const { manager } = await setupForExperimentFeature();
    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );
    // We're using the store in this test we need to wait for it to load
    await manager.store.ready();

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.equal(
      featureInstance.getAllVariables({ defaultValues: { screens: null } })
        .screens,
      null,
      "should return defaultValues param over default pref settings"
    );
  }
);
