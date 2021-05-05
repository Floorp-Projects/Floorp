"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);
const { Sampling } = ChromeUtils.import(
  "resource://gre/modules/components-utils/Sampling.jsm"
);
const { ClientEnvironment } = ChromeUtils.import(
  "resource://normandy/lib/ClientEnvironment.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

// Experiment store caches in prefs Enrollments for fast sync access
function cleanupStorePrefCache() {
  const SYNC_DATA_PREF_BRANCH = "nimbus.syncdatastore.";
  try {
    Services.prefs.deleteBranch(SYNC_DATA_PREF_BRANCH);
  } catch (e) {
    // Expected if nothing is cached
  }
}

/**
 * The normal case: Enrollment of a new experiment
 */
add_task(async function test_add_to_store() {
  const manager = ExperimentFakes.manager();
  const recipe = ExperimentFakes.recipe("foo");

  await manager.onStartup();

  await manager.enroll(recipe);
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
});

add_task(
  async function test_setExperimentActive_sendEnrollmentTelemetry_called() {
    const manager = ExperimentFakes.manager();
    const sandbox = sinon.createSandbox();
    sandbox.spy(manager, "setExperimentActive");
    sandbox.spy(manager, "sendEnrollmentTelemetry");

    await manager.onStartup();

    await manager.onStartup();

    await manager.enroll(ExperimentFakes.recipe("foo"));
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
  }
);

/**
 * Failure cases:
 * - slug conflict
 * - group conflict
 */

add_task(async function test_failure_name_conflict() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "sendFailureTelemetry");

  await manager.onStartup();

  // simulate adding a previouly enrolled experiment
  manager.store.addExperiment(ExperimentFakes.experiment("foo"));

  await Assert.rejects(
    manager.enroll(ExperimentFakes.recipe("foo")),
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
});

add_task(async function test_failure_group_conflict() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "sendFailureTelemetry");

  await manager.onStartup();

  // Two conflicting branches that both have the group "pink"
  // These should not be allowed to exist simultaneously.
  const existingBranch = {
    slug: "treatment",
    feature: { featureId: "pink", enabled: true },
  };
  const newBranch = {
    slug: "treatment",
    feature: { featureId: "pink", enabled: true },
  };

  // simulate adding an experiment with a conflicting group "pink"
  manager.store.addExperiment(
    ExperimentFakes.experiment("foo", {
      branch: existingBranch,
    })
  );

  // ensure .enroll chooses the special branch with the conflict
  sandbox.stub(manager, "chooseBranch").returns(newBranch);
  Assert.equal(
    await manager.enroll(
      ExperimentFakes.recipe("bar", { branches: [newBranch] })
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

  const SYNC_DATA_PREF_BRANCH = "nimbus.syncdatastore.";
  let dir = await OS.File.getCurrentDirectory();
  let src = OS.Path.join(dir, "reference_aboutwelcome_experiment_content.json");
  let bytes = await OS.File.read(src);
  const decoder = new TextDecoder();
  const content = JSON.parse(decoder.decode(bytes));
  // Create two dummy branches with the content from disk
  const branches = ["treatment-a", "treatment-b"].map(slug => ({
    slug,
    ratio: 1,
    feature: { value: content, enabled: true, featureId: "aboutwelcome" },
  }));
  let recipe = ExperimentFakes.recipe("reference-aw", { branches });
  // Ensure we get enrolled
  recipe.bucketConfig.count = recipe.bucketConfig.total;

  const manager = ExperimentFakes.manager();
  await manager.onStartup();
  await manager.enroll(recipe);

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
});

add_task(async function test_forceEnroll_cleanup() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  let unenrollStub = sandbox.spy(manager, "unenroll");
  let existingRecipe = ExperimentFakes.recipe("foo", {
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        feature: { featureId: "force-enrollment", enabled: true },
      },
    ],
  });
  let forcedRecipe = ExperimentFakes.recipe("bar", {
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        feature: { featureId: "force-enrollment", enabled: true },
      },
    ],
  });

  await manager.onStartup();
  await manager.enroll(existingRecipe);

  let setExperimentActiveSpy = sandbox.spy(manager, "setExperimentActive");
  manager.forceEnroll(forcedRecipe, forcedRecipe.branches[0]);

  Assert.ok(unenrollStub.called, "Unenrolled from existing experiment");
  Assert.equal(
    unenrollStub.firstCall.args[0],
    existingRecipe.slug,
    "Called with existing recipe slug"
  );
  Assert.ok(setExperimentActiveSpy.calledOnce, "Activated forced experiment");
  Assert.equal(
    setExperimentActiveSpy.firstCall.args[0].slug,
    forcedRecipe.slug,
    "Called with forced experiment slug"
  );
  Assert.equal(
    manager.store.getExperimentForFeature("force-enrollment").slug,
    forcedRecipe.slug,
    "Enrolled in forced experiment"
  );

  sandbox.restore();
});

add_task(async function test_updateEnrollment_skip_force() {
  const manager = ExperimentFakes.manager();
  let recipe = ExperimentFakes.recipe("foo");
  const sandbox = sinon.createSandbox();
  const updateEnrollmentSpy = sandbox.spy(manager, "updateEnrollment");
  const unenrollSpy = sandbox.spy(manager, "unenroll");

  await manager.onStartup();

  await manager.enroll(recipe);

  Assert.ok(manager.store.has("foo"), "Finished enrollment");

  // Something about the experiment change and we won't fit in the same
  // branch assignment
  await manager.onRecipe(
    { ...recipe, branches: [] },
    "test_ExperimentManager_enroll"
  );

  Assert.ok(
    updateEnrollmentSpy.calledOnce,
    "Update enrollement is called because we have the same slug"
  );
  Assert.ok(unenrollSpy.calledOnce, "Because no matching branch is found");
  Assert.ok(unenrollSpy.firstCall.args[0], "foo");

  updateEnrollmentSpy.resetHistory();
  unenrollSpy.resetHistory();

  recipe = ExperimentFakes.recipe("bar");
  await manager.forceEnroll(recipe, recipe.branches[0]);

  Assert.ok(manager.store.has("bar"), "Finished enrollment");

  // Something about the experiment change and we won't fit in the same
  // branch assignment but this time it's on a forced enrollment
  await manager.onRecipe(
    { ...recipe, branches: [] },
    "test_ExperimentManager_enroll"
  );

  Assert.ok(
    updateEnrollmentSpy.calledOnce,
    "Update enrollement is called because we have the same slug"
  );
  Assert.ok(unenrollSpy.notCalled, "Because this is a forced enrollment");
});
