"use strict";

const { ExperimentAPI, ExperimentFeature } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

async function setupForExperimentFeature() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  return { sandbox, manager };
}

function setDefaultBranch(pref, value) {
  let branch = Services.prefs.getDefaultBranch("");
  branch.setStringPref(pref, value);
}

const TEST_FALLBACK_PREF = "testprefbranch.config";
const FAKE_FEATURE_MANIFEST = {
  enabledFallbackPref: "testprefbranch.enabled",
  variables: {
    config: {
      type: "json",
      fallbackPref: TEST_FALLBACK_PREF,
    },
  },
};
const FAKE_FEATURE_REMOTE_VALUE = {
  variables: {},
  enabled: true,
  targeting: "true",
};

/**
 * # ExperimentFeature.getValue
 */
add_task(async function test_ExperimentFeature_ready() {
  const { sandbox, manager } = await setupForExperimentFeature();
  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  let readyPromise = featureInstance.ready();
  let stub = sandbox.stub();

  featureInstance.onUpdate(stub);

  const expected = ExperimentFakes.experiment("anexperiment", {
    branch: {
      slug: "treatment",
      feature: {
        featureId: "foo",
        enabled: true,
        value: { whoa: true },
      },
    },
  });

  manager.store.addExperiment(expected);

  await readyPromise;

  Assert.deepEqual(
    featureInstance.getValue(),
    { whoa: true },
    "should return getValue after waiting on ready"
  );
  Assert.equal(stub.callCount, 1, "Called when experiment registered");
  Assert.equal(stub.firstCall.args[1], "experiment-updated", "Verify reason");

  Services.prefs.clearUserPref("testprefbranch.value");
  sandbox.restore();
});

/**
 * # ExperimentFeature.getValue
 */
add_task(async function test_ExperimentFeature_getValue() {
  const { sandbox } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

  Services.prefs.setStringPref(TEST_FALLBACK_PREF, `{"bar": 123}`);

  Assert.deepEqual(
    featureInstance.getValue().config,
    { bar: 123 },
    "should return the fallback pref value"
  );

  Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
  sandbox.restore();
});

add_task(
  async function test_ExperimentFeature_getValue_prefer_experiment_over_default() {
    const { sandbox, manager } = await setupForExperimentFeature();

    const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

    const expected = ExperimentFakes.experiment("anexperiment", {
      branch: {
        slug: "treatment",
        feature: {
          featureId: "foo",
          enabled: true,
          value: { whoa: true },
        },
      },
    });

    manager.store.addExperiment(expected);

    setDefaultBranch(TEST_FALLBACK_PREF, `{"bar": 123}`);

    Assert.deepEqual(
      featureInstance.getValue(),
      { whoa: true },
      "should return the experiment feature value, not the fallback one."
    );

    Services.prefs.clearUserPref("testprefbranch.value");
    sandbox.restore();
  }
);

/**
 * # ExperimentFeature.isEnabled
 */

add_task(async function test_ExperimentFeature_isEnabled_default() {
  const { sandbox } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

  const noPrefFeature = new ExperimentFeature("bar", {});

  Assert.equal(
    noPrefFeature.isEnabled(),
    null,
    "should return null if no default pref branch is configured"
  );

  Services.prefs.clearUserPref("testprefbranch.enabled");

  Assert.equal(
    featureInstance.isEnabled(),
    null,
    "should return null if no default value or pref is set"
  );

  Assert.equal(
    featureInstance.isEnabled({ defaultValue: false }),
    false,
    "should use the default value param if no pref is set"
  );

  Services.prefs.setBoolPref("testprefbranch.enabled", false);

  Assert.equal(
    featureInstance.isEnabled({ defaultValue: true }),
    false,
    "should use the default pref value, including if it is false"
  );

  Services.prefs.clearUserPref("testprefbranch.enabled");
  sandbox.restore();
});

add_task(async function test_ExperimentFeature_isEnabled_remote_over_default() {
  const { manager, sandbox } = await setupForExperimentFeature();
  await manager.store.ready();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

  Services.prefs.setBoolPref("testprefbranch.enabled", false);

  Assert.equal(
    featureInstance.isEnabled(),
    false,
    "should use the default pref value, including if it is false"
  );

  manager.store.updateRemoteConfigs("foo", {
    ...FAKE_FEATURE_REMOTE_VALUE,
    enabled: true,
  });

  await featureInstance.ready();

  Assert.equal(
    featureInstance.isEnabled(),
    true,
    "should use the remote value over the default"
  );

  Services.prefs.clearUserPref("testprefbranch.enabled");
  sandbox.restore();
});

add_task(async function test_ExperimentFeature_test_helper_ready() {
  const { manager } = await setupForExperimentFeature();
  await manager.store.ready();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

  await ExperimentFakes.remoteDefaultsHelper({
    feature: featureInstance,
    store: manager.store,
    configuration: { variables: { remoteValue: "mochitest" }, enabled: true },
  });

  Assert.equal(featureInstance.isEnabled(), true, "enabled by remote config");
  Assert.equal(
    featureInstance.getValue().remoteValue,
    "mochitest",
    "set by remote config"
  );
});

add_task(
  async function test_ExperimentFeature_isEnabled_prefer_experiment_over_remote() {
    const { sandbox, manager } = await setupForExperimentFeature();
    const expected = ExperimentFakes.experiment("foo", {
      branch: {
        slug: "treatment",
        feature: {
          featureId: "foo",
          enabled: true,
          value: null,
        },
      },
    });
    const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

    await manager.store.ready();

    manager.store.addExperiment(expected);

    const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
    Services.prefs.setBoolPref("testprefbranch.enabled", false);
    manager.store.updateRemoteConfigs("foo", {
      ...FAKE_FEATURE_REMOTE_VALUE,
      enabled: false,
    });

    await featureInstance.ready();

    Assert.equal(
      featureInstance.isEnabled(),
      true,
      "should return the enabled value defined in the experiment, not the default pref or the remote value"
    );

    Assert.ok(exposureSpy.notCalled, "should emit exposure by default event");

    featureInstance.isEnabled({ sendExposureEvent: true });

    Assert.ok(exposureSpy.calledOnce, "should emit exposure event");

    Services.prefs.clearUserPref("testprefbranch.enabled");
    sandbox.restore();
  }
);

add_task(async function test_ExperimentFeature_isEnabled_no_exposure() {
  const { sandbox, manager } = await setupForExperimentFeature();
  const expected = ExperimentFakes.experiment("blah", {
    branch: {
      slug: "treatment",
      feature: {
        featureId: "foo",
        enabled: false,
        value: null,
      },
    },
  });
  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  manager.store.addExperiment(expected);

  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");

  const actual = featureInstance.isEnabled({ sendExposureEvent: false });

  Assert.deepEqual(actual, false, "should return feature as disabled");
  Assert.ok(
    exposureSpy.notCalled,
    "should not emit an exposure event when options = { sendExposureEvent: false}"
  );

  sandbox.restore();
});

add_task(async function test_record_exposure_event() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  featureInstance.recordExposureEvent();

  Assert.ok(
    exposureSpy.notCalled,
    "should not emit an exposure event when no experiment is active"
  );

  manager.store.addExperiment(
    ExperimentFakes.experiment("blah", {
      featureIds: ["foo"],
      branch: {
        slug: "treatment",
        feature: {
          featureId: "foo",
          enabled: false,
          value: null,
        },
      },
    })
  );

  featureInstance.recordExposureEvent();

  Assert.ok(
    exposureSpy.calledOnce,
    "should emit an exposure event when there is an experiment"
  );

  sandbox.restore();
});

add_task(async function test_record_exposure_event_once() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  manager.store.addExperiment(
    ExperimentFakes.experiment("blah", {
      featureIds: ["foo"],
      branch: {
        slug: "treatment",
        feature: {
          featureId: "foo",
          enabled: false,
          value: null,
        },
      },
    })
  );

  featureInstance.recordExposureEvent();
  featureInstance.recordExposureEvent();
  featureInstance.recordExposureEvent();

  Assert.ok(exposureSpy.calledOnce, "Should emit a single exposure event.");

  sandbox.restore();
});

add_task(async function test_prevent_double_exposure_getValue() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  manager.store.addExperiment(
    ExperimentFakes.experiment("blah", {
      featureIds: ["foo"],
      branch: {
        slug: "treatment",
        feature: {
          featureId: "foo",
          enabled: false,
          value: null,
        },
      },
    })
  );

  featureInstance.getValue({ sendExposureEvent: true });
  featureInstance.getValue({ sendExposureEvent: true });
  featureInstance.getValue({ sendExposureEvent: true });

  Assert.ok(
    exposureSpy.calledOnce,
    "Should emit a single exposure event (getValue)."
  );

  sandbox.restore();
});

add_task(async function test_prevent_double_exposure_isEnabled() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  manager.store.addExperiment(
    ExperimentFakes.experiment("blah", {
      featureIds: ["foo"],
      branch: {
        slug: "treatment",
        feature: {
          featureId: "foo",
          enabled: false,
          value: null,
        },
      },
    })
  );

  featureInstance.isEnabled({ sendExposureEvent: true });
  featureInstance.isEnabled({ sendExposureEvent: true });
  featureInstance.isEnabled({ sendExposureEvent: true });

  Assert.ok(
    exposureSpy.calledOnce,
    "Should emit a single exposure event (getValue)."
  );

  sandbox.restore();
});
