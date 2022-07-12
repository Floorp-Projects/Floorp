"use strict";

const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);
const { Sampling } = ChromeUtils.import(
  "resource://gre/modules/components-utils/Sampling.jsm"
);
const { ClientEnvironment } = ChromeUtils.import(
  "resource://normandy/lib/ClientEnvironment.jsm"
);
const { cleanupStorePrefCache } = ExperimentFakes;

const { ExperimentStore } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentStore.jsm"
);
const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
const { TelemetryEvents } = ChromeUtils.import(
  "resource://normandy/lib/TelemetryEvents.jsm"
);

const { SYNC_DATA_PREF_BRANCH, SYNC_DEFAULTS_PREF_BRANCH } = ExperimentStore;

const globalSandbox = sinon.createSandbox();
globalSandbox.spy(TelemetryEnvironment, "setExperimentInactive");
globalSandbox.spy(TelemetryEvents, "sendEvent");
registerCleanupFunction(() => {
  globalSandbox.restore();
});

/**
 * FOG requires a little setup in order to test it
 */
add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

/**
 * The normal case: Enrollment of a new experiment
 */
add_task(async function test_add_to_store() {
  const manager = ExperimentFakes.manager();
  const recipe = ExperimentFakes.recipe("foo");
  const enrollPromise = new Promise(resolve =>
    manager.store.on("update:foo", resolve)
  );

  await manager.onStartup();

  await manager.enroll(recipe, "test_add_to_store");
  await enrollPromise;
  const experiment = manager.store.get("foo");

  Assert.ok(experiment, "should add an experiment with slug foo");
  Assert.ok(
    recipe.branches.includes(experiment.branch),
    "should choose a branch from the recipe.branches"
  );
  Assert.equal(experiment.active, true, "should set .active = true");
  Assert.ok(
    NormandyTestUtils.isUuid(experiment.enrollmentId),
    "should add a valid enrollmentId"
  );

  manager.unenroll("foo", "test-cleanup");
});

add_task(async function test_add_rollout_to_store() {
  const manager = ExperimentFakes.manager();
  const recipe = {
    ...ExperimentFakes.recipe("rollout-slug"),
    branches: [ExperimentFakes.rollout("rollout").branch],
    isRollout: true,
    active: true,
    bucketConfig: {
      namespace: "nimbus-test-utils",
      randomizationUnit: "normandy_id",
      start: 0,
      count: 1000,
      total: 1000,
    },
  };
  const enrollPromise = new Promise(resolve =>
    manager.store.on("update:rollout-slug", resolve)
  );

  await manager.onStartup();

  await manager.enroll(recipe, "test_add_rollout_to_store");
  await enrollPromise;
  const experiment = manager.store.get("rollout-slug");

  Assert.ok(experiment, `Should add an experiment with slug ${recipe.slug}`);
  Assert.ok(
    recipe.branches.includes(experiment.branch),
    "should choose a branch from the recipe.branches"
  );
  Assert.equal(experiment.isRollout, true, "should have .isRollout");

  manager.unenroll("rollout-slug", "test-cleanup");
});

add_task(
  async function test_setExperimentActive_sendEnrollmentTelemetry_called() {
    const manager = ExperimentFakes.manager();
    const sandbox = sinon.createSandbox();
    const enrollPromise = new Promise(resolve =>
      manager.store.on("update:foo", resolve)
    );
    sandbox.spy(manager, "setExperimentActive");
    sandbox.spy(manager, "sendEnrollmentTelemetry");

    // Clear any pre-existing data in Glean
    Services.fog.testResetFOG();

    await manager.onStartup();

    // Ensure there is no experiment active with the id in FOG
    Assert.equal(
      undefined,
      Services.fog.testGetExperimentData("foo"),
      "no active experiment exists before enrollment"
    );

    // Check that there aren't any Glean enrollment events yet
    var enrollmentEvents = Glean.nimbusEvents.enrollment.testGetValue();
    Assert.equal(
      undefined,
      enrollmentEvents,
      "no Glean enrollment events before enrollment"
    );

    await manager.enroll(
      ExperimentFakes.recipe("foo"),
      "test_setExperimentActive_sendEnrollmentTelemetry_called"
    );
    await enrollPromise;
    const experiment = manager.store.get("foo");

    Assert.equal(
      manager.setExperimentActive.calledWith(experiment),
      true,
      "should call setExperimentActive after an enrollment"
    );

    Assert.equal(
      manager.sendEnrollmentTelemetry.calledWith(experiment),
      true,
      "should call sendEnrollmentTelemetry after an enrollment"
    );

    // Test Glean experiment API interaction
    Assert.notEqual(
      undefined,
      Services.fog.testGetExperimentData(experiment.slug),
      "Glean.setExperimentActive called with `foo` feature"
    );

    // Check that the Glean enrollment event was recorded.
    enrollmentEvents = Glean.nimbusEvents.enrollment.testGetValue();
    // We expect only one event
    Assert.equal(1, enrollmentEvents.length);
    // And that one event matches the expected enrolled experiment
    Assert.equal(
      experiment.slug,
      enrollmentEvents[0].extra.experiment,
      "Glean.nimbusEvents.enrollment recorded with correct experiment slug"
    );
    Assert.equal(
      experiment.branch.slug,
      enrollmentEvents[0].extra.branch,
      "Glean.nimbusEvents.enrollment recorded with correct branch slug"
    );
    Assert.equal(
      experiment.experimentType,
      enrollmentEvents[0].extra.experiment_type,
      "Glean.nimbusEvents.enrollment recorded with correct experiment type"
    );
    Assert.equal(
      experiment.enrollmentId,
      enrollmentEvents[0].extra.enrollment_id,
      "Glean.nimbusEvents.enrollment recorded with correct enrollment id"
    );

    manager.unenroll("foo", "test-cleanup");
  }
);

add_task(async function test_setRolloutActive_sendEnrollmentTelemetry_called() {
  globalSandbox.reset();
  globalSandbox.spy(TelemetryEnvironment, "setExperimentActive");
  globalSandbox.spy(TelemetryEvents.sendEvent);
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const rolloutRecipe = {
    ...ExperimentFakes.recipe("rollout"),
    branches: [ExperimentFakes.rollout("rollout").branch],
    isRollout: true,
  };
  const enrollPromise = new Promise(resolve =>
    manager.store.on("update:rollout", resolve)
  );
  sandbox.spy(manager, "setExperimentActive");
  sandbox.spy(manager, "sendEnrollmentTelemetry");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();

  // Test Glean experiment API interaction
  Assert.equal(
    undefined,
    Services.fog.testGetExperimentData("rollout"),
    "no rollout active before enrollment"
  );

  // Check that there aren't any Glean enrollment events yet
  var enrollmentEvents = Glean.nimbusEvents.enrollment.testGetValue();
  Assert.equal(
    undefined,
    enrollmentEvents,
    "no Glean enrollment events before enrollment"
  );

  let result = await manager.enroll(
    rolloutRecipe,
    "test_setRolloutActive_sendEnrollmentTelemetry_called"
  );

  await enrollPromise;

  const enrollment = manager.store.get("rollout");

  Assert.ok(!!result && !!enrollment, "Enrollment was successful");

  Assert.equal(
    TelemetryEnvironment.setExperimentActive.called,
    true,
    "should call setExperimentActive"
  );
  Assert.ok(
    manager.setExperimentActive.calledWith(enrollment),
    "Should call setExperimentActive with the rollout"
  );
  Assert.equal(
    manager.setExperimentActive.firstCall.args[0].experimentType,
    "rollout",
    "Should have the correct experimentType"
  );
  Assert.equal(
    manager.sendEnrollmentTelemetry.calledWith(enrollment),
    true,
    "should call sendEnrollmentTelemetry after an enrollment"
  );
  Assert.ok(
    TelemetryEvents.sendEvent.calledOnce,
    "Should send out enrollment telemetry"
  );
  Assert.ok(
    TelemetryEvents.sendEvent.calledWith(
      "enroll",
      sinon.match.string,
      enrollment.slug,
      {
        experimentType: "rollout",
        branch: enrollment.branch.slug,
        enrollmentId: enrollment.enrollmentId,
      }
    ),
    "Should send telemetry with expected values"
  );

  // Test Glean experiment API interaction
  Assert.equal(
    enrollment.branch.slug,
    Services.fog.testGetExperimentData(enrollment.slug).branch,
    "Glean.setExperimentActive called with expected values"
  );

  // Check that the Glean enrollment event was recorded.
  enrollmentEvents = Glean.nimbusEvents.enrollment.testGetValue();
  // We expect only one event
  Assert.equal(1, enrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    enrollment.slug,
    enrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.enrollment recorded with correct experiment slug"
  );
  Assert.equal(
    enrollment.branch.slug,
    enrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.enrollment recorded with correct branch slug"
  );
  Assert.equal(
    enrollment.experimentType,
    enrollmentEvents[0].extra.experiment_type,
    "Glean.nimbusEvents.enrollment recorded with correct experiment type"
  );
  Assert.equal(
    enrollment.enrollmentId,
    enrollmentEvents[0].extra.enrollment_id,
    "Glean.nimbusEvents.enrollment recorded with correct enrollment id"
  );

  manager.unenroll("rollout", "test-cleanup");

  globalSandbox.restore();
});

// /**
//  * Failure cases:
//  * - slug conflict
//  * - group conflict
//  */

add_task(async function test_failure_name_conflict() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "sendFailureTelemetry");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();

  // Check that there aren't any Glean enroll_failed events yet
  var failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue();
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  // simulate adding a previouly enrolled experiment
  await manager.store.addEnrollment(ExperimentFakes.experiment("foo"));

  await Assert.rejects(
    manager.enroll(ExperimentFakes.recipe("foo"), "test_failure_name_conflict"),
    /An experiment with the slug "foo" already exists/,
    "should throw if a conflicting experiment exists"
  );

  Assert.equal(
    manager.sendFailureTelemetry.calledWith(
      "enrollFailed",
      "foo",
      "name-conflict"
    ),
    true,
    "should send failure telemetry if a conflicting experiment exists"
  );

  // Check that the Glean enrollment event was recorded.
  failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue();
  // We expect only one event
  Assert.equal(1, failureEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    "foo",
    failureEvents[0].extra.experiment,
    "Glean.nimbusEvents.enroll_failed recorded with correct experiment slug"
  );
  Assert.equal(
    "name-conflict",
    failureEvents[0].extra.reason,
    "Glean.nimbusEvents.enroll_failed recorded with correct reason"
  );

  manager.unenroll("foo", "test-cleanup");
});

add_task(async function test_failure_group_conflict() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "sendFailureTelemetry");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();

  // Check that there aren't any Glean enroll_failed events yet
  var failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue();
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  // Two conflicting branches that both have the group "pink"
  // These should not be allowed to exist simultaneously.
  const existingBranch = {
    slug: "treatment",
    features: [{ featureId: "pink", value: {} }],
  };
  const newBranch = {
    slug: "treatment",
    features: [{ featureId: "pink", value: {} }],
  };

  // simulate adding an experiment with a conflicting group "pink"
  await manager.store.addEnrollment(
    ExperimentFakes.experiment("foo", {
      branch: existingBranch,
    })
  );

  // ensure .enroll chooses the special branch with the conflict
  sandbox.stub(manager, "chooseBranch").returns(newBranch);
  Assert.equal(
    await manager.enroll(
      ExperimentFakes.recipe("bar", { branches: [newBranch] }),
      "test_failure_group_conflict"
    ),
    null,
    "should not enroll if there is a feature conflict"
  );

  Assert.equal(
    manager.sendFailureTelemetry.calledWith(
      "enrollFailed",
      "bar",
      "feature-conflict"
    ),
    true,
    "should send failure telemetry if a feature conflict exists"
  );

  // Check that the Glean enroll_failed event was recorded.
  failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue();
  // We expect only one event
  Assert.equal(1, failureEvents.length);
  // And that event matches the expected experiment and reason
  Assert.equal(
    "bar",
    failureEvents[0].extra.experiment,
    "Glean.nimbusEvents.enroll_failed recorded with correct experiment slug"
  );
  Assert.equal(
    "feature-conflict",
    failureEvents[0].extra.reason,
    "Glean.nimbusEvents.enroll_failed recorded with correct reason"
  );

  manager.unenroll("foo", "test-cleanup");
});

add_task(async function test_rollout_failure_group_conflict() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const rollout = ExperimentFakes.rollout("rollout-enrollment");
  const recipe = {
    ...ExperimentFakes.recipe("rollout-recipe"),
    branches: [rollout.branch],
    isRollout: true,
  };
  sandbox.spy(manager, "sendFailureTelemetry");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();

  // Check that there aren't any Glean enroll_failed events yet
  var failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue();
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  // simulate adding an experiment with a conflicting group "pink"
  await manager.store.addEnrollment(rollout);

  Assert.equal(
    await manager.enroll(recipe, "test_rollout_failure_group_conflict"),
    null,
    "should not enroll if there is a feature conflict"
  );

  Assert.equal(
    manager.sendFailureTelemetry.calledWith(
      "enrollFailed",
      recipe.slug,
      "feature-conflict"
    ),
    true,
    "should send failure telemetry if a feature conflict exists"
  );

  // Check that the Glean enroll_failed event was recorded.
  failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue();
  // We expect only one event
  Assert.equal(1, failureEvents.length);
  // And that event matches the expected experiment and reason
  Assert.equal(
    recipe.slug,
    failureEvents[0].extra.experiment,
    "Glean.nimbusEvents.enroll_failed recorded with correct experiment slug"
  );
  Assert.equal(
    "feature-conflict",
    failureEvents[0].extra.reason,
    "Glean.nimbusEvents.enroll_failed recorded with correct reason"
  );

  manager.unenroll("rollout-enrollment", "test-cleanup");
});

add_task(async function test_rollout_experiment_no_conflict() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const experiment = ExperimentFakes.experiment("rollout-enrollment");
  const recipe = {
    ...ExperimentFakes.recipe("rollout-recipe"),
    branches: [experiment.branch],
    isRollout: true,
  };
  sandbox.spy(manager, "sendFailureTelemetry");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();

  // Check that there aren't any Glean enroll_failed events yet
  var failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue();
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  await manager.store.addEnrollment(experiment);

  Assert.equal(
    (await manager.enroll(recipe, "test_rollout_failure_group_conflict"))?.slug,
    recipe.slug,
    "Experiment and Rollouts can exists for the same feature"
  );

  Assert.ok(
    manager.sendFailureTelemetry.notCalled,
    "Should send failure telemetry if a feature conflict exists"
  );

  // Check that there aren't any Glean enroll_failed events
  failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue();
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  manager.unenroll("rollout-enrollment", "test-cleanup");
});

add_task(async function test_sampling_check() {
  const manager = ExperimentFakes.manager();
  let recipe = ExperimentFakes.recipe("foo", { bucketConfig: null });
  const sandbox = sinon.createSandbox();
  sandbox.stub(Sampling, "bucketSample").resolves(true);
  sandbox.replaceGetter(ClientEnvironment, "userId", () => 42);

  Assert.ok(
    !manager.isInBucketAllocation(recipe.bucketConfig),
    "fails for no bucket config"
  );

  recipe = ExperimentFakes.recipe("foo2", {
    bucketConfig: { randomizationUnit: "foo" },
  });

  Assert.ok(
    !manager.isInBucketAllocation(recipe.bucketConfig),
    "fails for unknown randomizationUnit"
  );

  recipe = ExperimentFakes.recipe("foo3");

  const result = await manager.isInBucketAllocation(recipe.bucketConfig);

  Assert.equal(
    Sampling.bucketSample.callCount,
    1,
    "it should call bucketSample"
  );
  Assert.ok(result, "result should be true");
  const { args } = Sampling.bucketSample.firstCall;
  Assert.equal(args[0][0], 42, "called with expected randomization id");
  Assert.equal(
    args[0][1],
    recipe.bucketConfig.namespace,
    "called with expected namespace"
  );
  Assert.equal(
    args[1],
    recipe.bucketConfig.start,
    "called with expected start"
  );
  Assert.equal(
    args[2],
    recipe.bucketConfig.count,
    "called with expected count"
  );
  Assert.equal(
    args[3],
    recipe.bucketConfig.total,
    "called with expected total"
  );

  sandbox.reset();
});

add_task(async function enroll_in_reference_aw_experiment() {
  cleanupStorePrefCache();

  let dir = Services.dirsvc.get("CurWorkD", Ci.nsIFile).path;
  let src = PathUtils.join(
    dir,
    "reference_aboutwelcome_experiment_content.json"
  );
  const content = await IOUtils.readJSON(src);
  // Create two dummy branches with the content from disk
  const branches = ["treatment-a", "treatment-b"].map(slug => ({
    slug,
    ratio: 1,
    features: [
      { value: { ...content, enabled: true }, featureId: "aboutwelcome" },
    ],
  }));
  let recipe = ExperimentFakes.recipe("reference-aw", { branches });
  // Ensure we get enrolled
  recipe.bucketConfig.count = recipe.bucketConfig.total;

  const manager = ExperimentFakes.manager();
  const enrollPromise = new Promise(resolve =>
    manager.store.on("update:reference-aw", resolve)
  );
  await manager.onStartup();
  await manager.enroll(recipe, "enroll_in_reference_aw_experiment");
  await enrollPromise;

  Assert.ok(manager.store.get("reference-aw"), "Successful onboarding");
  let prefValue = Services.prefs.getStringPref(
    `${SYNC_DATA_PREF_BRANCH}aboutwelcome`
  );
  Assert.ok(
    prefValue,
    "aboutwelcome experiment enrollment should be stored to prefs"
  );
  // In case some regression causes us to store a significant amount of data
  // in prefs.
  Assert.ok(prefValue.length < 3498, "Make sure we don't bloat the prefs");

  manager.unenroll(recipe.slug, "enroll_in_reference_aw_experiment:cleanup");
  manager.store._deleteForTests("aboutwelcome");
});

add_task(async function test_forceEnroll_cleanup() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const fooEnrollPromise = new Promise(resolve =>
    manager.store.on("update:foo", resolve)
  );
  const barEnrollPromise = new Promise(resolve =>
    manager.store.on("update:optin-bar", resolve)
  );
  let unenrollStub = sandbox.spy(manager, "unenroll");
  let existingRecipe = ExperimentFakes.recipe("foo", {
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        features: [{ featureId: "force-enrollment", value: {} }],
      },
    ],
  });
  let forcedRecipe = ExperimentFakes.recipe("bar", {
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        features: [{ featureId: "force-enrollment", value: {} }],
      },
    ],
  });

  await manager.onStartup();
  await manager.enroll(existingRecipe, "test_forceEnroll_cleanup");
  await fooEnrollPromise;

  let setExperimentActiveSpy = sandbox.spy(manager, "setExperimentActive");
  manager.forceEnroll(forcedRecipe, forcedRecipe.branches[0]);
  await barEnrollPromise;

  Assert.ok(unenrollStub.called, "Unenrolled from existing experiment");
  Assert.equal(
    unenrollStub.firstCall.args[0],
    existingRecipe.slug,
    "Called with existing recipe slug"
  );
  Assert.ok(setExperimentActiveSpy.calledOnce, "Activated forced experiment");
  Assert.equal(
    setExperimentActiveSpy.firstCall.args[0].slug,
    `optin-${forcedRecipe.slug}`,
    "Called with forced experiment slug"
  );
  Assert.equal(
    manager.store.getExperimentForFeature("force-enrollment").slug,
    `optin-${forcedRecipe.slug}`,
    "Enrolled in forced experiment"
  );

  manager.unenroll(`optin-${forcedRecipe.slug}`, "test-cleanup");

  sandbox.restore();
});

add_task(async function test_rollout_unenroll_conflict() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  let unenrollStub = sandbox.stub(manager, "unenroll").returns(true);
  let enrollStub = sandbox.stub(manager, "_enroll").returns(true);
  let rollout = ExperimentFakes.rollout("rollout_conflict");

  // We want to force a conflict
  sandbox.stub(manager.store, "getRolloutForFeature").returns(rollout);

  manager.forceEnroll(rollout, rollout.branch);

  Assert.ok(unenrollStub.calledOnce, "Should unenroll the conflicting rollout");
  Assert.ok(
    unenrollStub.calledWith(rollout.slug, "force-enrollment"),
    "Should call with expected slug"
  );
  Assert.ok(enrollStub.calledOnce, "Should call enroll as expected");

  sandbox.restore();
});

add_task(async function test_featureIds_is_stored() {
  Services.prefs.setStringPref("messaging-system.log", "all");
  const recipe = ExperimentFakes.recipe("featureIds");
  // Ensure we get enrolled
  recipe.bucketConfig.count = recipe.bucketConfig.total;
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);

  await manager.onStartup();

  const {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(recipe, { manager });

  await enrollmentPromise;

  Assert.ok(manager.store.addEnrollment.calledOnce, "experiment is stored");
  let [enrollment] = manager.store.addEnrollment.firstCall.args;
  Assert.ok("featureIds" in enrollment, "featureIds is stored");
  Assert.deepEqual(
    enrollment.featureIds,
    ["testFeature"],
    "Has expected value"
  );

  await doExperimentCleanup();
});

add_task(async function experiment_and_rollout_enroll_and_cleanup() {
  let store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);

  await manager.onStartup();

  let rolloutCleanup = await ExperimentFakes.enrollWithRollout(
    {
      featureId: "aboutwelcome",
      value: { enabled: true },
    },
    {
      manager,
    }
  );

  let experimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: "aboutwelcome",
      value: { enabled: true },
    },
    { manager }
  );

  Assert.ok(
    Services.prefs.getBoolPref(`${SYNC_DATA_PREF_BRANCH}aboutwelcome.enabled`)
  );
  Assert.ok(
    Services.prefs.getBoolPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome.enabled`
    )
  );

  await experimentCleanup();

  Assert.ok(
    !Services.prefs.getBoolPref(
      `${SYNC_DATA_PREF_BRANCH}aboutwelcome.enabled`,
      false
    )
  );
  Assert.ok(
    Services.prefs.getBoolPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome.enabled`
    )
  );

  await rolloutCleanup();

  Assert.ok(
    !Services.prefs.getBoolPref(
      `${SYNC_DATA_PREF_BRANCH}aboutwelcome.enabled`,
      false
    )
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome.enabled`,
      false
    )
  );
});
