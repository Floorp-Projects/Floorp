"use strict";

const {
  ExperimentAPI,
  _ExperimentFeature: ExperimentFeature,
} = ChromeUtils.import("resource://nimbus/ExperimentAPI.jsm");
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
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
  description: "Test feature",
  exposureDescription: "Used in tests",
  variables: {
    enabled: {
      type: "boolean",
      fallbackPref: "testprefbranch.enabled",
    },
    config: {
      type: "json",
      fallbackPref: TEST_FALLBACK_PREF,
    },
    remoteValue: {
      type: "boolean",
    },
    test: {
      type: "boolean",
    },
    title: {
      type: "string",
    },
  },
};
const FAKE_FEATURE_REMOTE_VALUE = {
  value: {
    enabled: true,
  },
};

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

add_task(async function test_ExperimentFeature_isEnabled_default_over_remote() {
  const { manager, sandbox } = await setupForExperimentFeature();
  const rollout = ExperimentFakes.rollout("foo-rollout", {
    branch: {
      slug: "slug",
      features: [
        {
          featureId: "foo",
          enabled: true,
          value: FAKE_FEATURE_REMOTE_VALUE,
        },
      ],
    },
  });
  await manager.store.ready();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

  Services.prefs.setBoolPref("testprefbranch.enabled", false);

  Assert.equal(
    featureInstance.isEnabled(),
    false,
    "should use the default pref value, including if it is false"
  );

  await manager.store.addEnrollment(rollout);

  Assert.equal(
    featureInstance.isEnabled(),
    false,
    "Should still use userpref over remote"
  );

  Services.prefs.clearUserPref("testprefbranch.enabled");

  Assert.equal(
    featureInstance.isEnabled(),
    true,
    "Should use remote value over default pref"
  );

  sandbox.restore();
});

add_task(async function test_ExperimentFeature_test_helper_ready() {
  const { manager } = await setupForExperimentFeature();
  await manager.store.ready();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

  await ExperimentFakes.enrollWithRollout(
    {
      featureId: "foo",
      enabled: true,
      value: { remoteValue: "mochitest", enabled: true },
    },
    {
      manager,
    }
  );

  Assert.equal(featureInstance.isEnabled(), true, "enabled by remote config");
  Assert.equal(
    featureInstance.getVariable("remoteValue"),
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
        features: [
          {
            featureId: "foo",
            value: { enabled: true },
          },
        ],
      },
    });
    const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

    await manager.store.ready();
    await manager.store.addEnrollment(expected);

    const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");

    await ExperimentFakes.enrollWithRollout(
      {
        featureId: "foo",
        enabled: false,
        value: { enabled: false },
      },
      {
        manager,
      }
    );

    Assert.equal(
      featureInstance.isEnabled(),
      true,
      "should return the enabled value defined in the experiment not the remote value"
    );

    Services.prefs.setBoolPref("testprefbranch.enabled", false);

    Assert.equal(
      featureInstance.isEnabled(),
      false,
      "should return the user pref not the experiment value"
    );

    // Exposure is not triggered if user pref is set
    Services.prefs.clearUserPref("testprefbranch.enabled");

    Assert.ok(exposureSpy.notCalled, "should not emit exposure by default");

    featureInstance.recordExposureEvent();

    Assert.ok(exposureSpy.calledOnce, "should emit exposure event");

    sandbox.restore();
  }
);

add_task(
  async function test_ExperimentFeature_isEnabled_prefer_experiment_over_remote_legacy() {
    const { sandbox, manager } = await setupForExperimentFeature();
    const expected = ExperimentFakes.experiment("foo", {
      branch: {
        slug: "treatment",
        features: [
          {
            featureId: "foo",
            enabled: true,
            value: { legacy: true },
          },
        ],
      },
    });
    const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

    await manager.store.ready();
    await manager.store.addEnrollment(expected);

    await ExperimentFakes.enrollWithRollout(
      {
        featureId: "foo",
        enabled: false,
        value: { legacy: true, enabled: false },
      },
      {
        manager,
      }
    );

    Assert.equal(
      featureInstance.isEnabled(),
      true,
      "should return the enabled value defined in the experiment not the remote value"
    );

    sandbox.restore();
  }
);

add_task(async function test_record_exposure_event() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
  const getExperimentSpy = sandbox.spy(ExperimentAPI, "getExperimentMetaData");
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  featureInstance.recordExposureEvent();

  Assert.ok(
    exposureSpy.notCalled,
    "should not emit an exposure event when no experiment is active"
  );

  await manager.store.addEnrollment(
    ExperimentFakes.experiment("blah", {
      branch: {
        slug: "treatment",
        features: [
          {
            featureId: "foo",
            value: { enabled: false },
          },
        ],
      },
    })
  );

  featureInstance.recordExposureEvent();

  Assert.ok(
    exposureSpy.calledOnce,
    "should emit an exposure event when there is an experiment"
  );
  Assert.equal(getExperimentSpy.callCount, 2, "Should be called every time");

  sandbox.restore();
});

add_task(async function test_record_exposure_event_once() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  await manager.store.addEnrollment(
    ExperimentFakes.experiment("blah", {
      branch: {
        slug: "treatment",
        features: [
          {
            featureId: "foo",
            value: { enabled: false },
          },
        ],
      },
    })
  );

  featureInstance.recordExposureEvent({ once: true });
  featureInstance.recordExposureEvent({ once: true });
  featureInstance.recordExposureEvent({ once: true });

  Assert.ok(
    exposureSpy.calledOnce,
    "Should emit a single exposure event when the once param is true."
  );

  sandbox.restore();
});

add_task(async function test_allow_multiple_exposure_events() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: "foo",
      value: { enabled: false },
    },
    { manager }
  );

  featureInstance.recordExposureEvent();
  featureInstance.recordExposureEvent();
  featureInstance.recordExposureEvent();

  Assert.ok(exposureSpy.called, "Should emit exposure event");
  Assert.equal(
    exposureSpy.callCount,
    3,
    "Should emit an exposure event for each function call"
  );

  sandbox.restore();
  await doExperimentCleanup();
});

add_task(async function test_isEnabled_backwards_compatible() {
  const PREVIOUS_FEATURE_MANIFEST = {
    variables: {
      config: {
        type: "json",
        fallbackPref: TEST_FALLBACK_PREF,
      },
    },
  };
  let sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);
  const exposureSpy = sandbox.spy(ExperimentAPI, "recordExposureEvent");
  const feature = new ExperimentFeature("foo", PREVIOUS_FEATURE_MANIFEST);

  await manager.onStartup();

  await ExperimentFakes.enrollWithRollout(
    {
      featureId: "foo",
      value: { enabled: false },
    },
    {
      manager,
    }
  );

  Assert.ok(!feature.isEnabled(), "Disabled based on remote configs");

  await manager.store.addEnrollment(
    ExperimentFakes.experiment("blah", {
      branch: {
        slug: "treatment",
        features: [
          {
            featureId: "foo",
            enabled: true,
            value: {},
          },
        ],
      },
    })
  );

  Assert.ok(exposureSpy.notCalled, "Not called until now");
  Assert.ok(feature.isEnabled(), "Enabled based on experiment recipe");
});

add_task(async function test_onUpdate_before_store_ready() {
  let sandbox = sinon.createSandbox();
  const feature = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const stub = sandbox.stub();
  const manager = ExperimentFakes.manager();
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);
  sandbox.stub(manager.store, "getAllActive").returns([
    ExperimentFakes.experiment("foo-experiment", {
      branch: {
        slug: "control",
        features: [
          {
            featureId: "foo",
            value: null,
          },
        ],
      },
    }),
  ]);

  // We register for updates before the store finished loading experiments
  // from disk
  feature.onUpdate(stub);

  await manager.onStartup();

  Assert.ok(
    stub.calledOnce,
    "Called on startup after loading experiments from disk"
  );
  Assert.equal(
    stub.firstCall.args[1],
    "feature-experiment-loaded",
    "Called for the expected reason"
  );
});

add_task(async function test_ExperimentFeature_test_ready_late() {
  const { manager, sandbox } = await setupForExperimentFeature();
  const stub = sandbox.stub();

  const featureInstance = new ExperimentFeature(
    "test-feature",
    FAKE_FEATURE_MANIFEST
  );

  const rollout = ExperimentFakes.rollout("foo", {
    branch: {
      slug: "slug",
      features: [
        {
          featureId: featureInstance.featureId,
          value: {
            title: "hello",
            enabled: true,
          },
        },
      ],
    },
  });

  sandbox.stub(manager.store, "getAllRollouts").returns([rollout]);

  await manager.onStartup();

  featureInstance.onUpdate(stub);

  await featureInstance.ready();

  Assert.ok(stub.notCalled, "We register too late to catch any events");

  setDefaultBranch(TEST_FALLBACK_PREF, JSON.stringify({ foo: true }));

  Assert.deepEqual(
    featureInstance.getVariable("config"),
    { foo: true },
    "Feature is ready even when initialized after store update"
  );
  Assert.equal(
    featureInstance.getVariable("title"),
    "hello",
    "Returns the NimbusTestUtils rollout default value"
  );
});
