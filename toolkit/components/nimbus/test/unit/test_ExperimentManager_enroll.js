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
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const { cleanupStorePrefCache } = ExperimentFakes;

const { ExperimentStore } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentStore.jsm"
);

const { SYNC_DATA_PREF_BRANCH } = ExperimentStore;

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

    await manager.onStartup();

    await manager.onStartup();

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
  await manager.store.addExperiment(ExperimentFakes.experiment("foo"));

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
    feature: { featureId: "pink", enabled: true, value: {} },
  };
  const newBranch = {
    slug: "treatment",
    feature: { featureId: "pink", enabled: true, value: {} },
  };

  // simulate adding an experiment with a conflicting group "pink"
  await manager.store.addExperiment(
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
        feature: { featureId: "force-enrollment", enabled: true, value: {} },
      },
    ],
  });
  let forcedRecipe = ExperimentFakes.recipe("bar", {
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        feature: { featureId: "force-enrollment", enabled: true, value: {} },
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

  sandbox.restore();
});

add_task(async function test_featuremanifest_enum() {
  let recipe = ExperimentFakes.recipe("featuremanifest_enum_success", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        feature: {
          featureId: "privatebrowsing",
          value: { promoLinkType: "link" },
        },
      },
    ],
  });
  // Ensure we get enrolled
  recipe.bucketConfig.count = recipe.bucketConfig.total;
  let manager = ExperimentFakes.manager();

  await manager.onStartup();

  let { enrollmentPromise } = ExperimentFakes.enrollmentHelper(recipe, {
    manager,
  });

  await enrollmentPromise;

  Assert.ok(
    manager.store.hasExperimentForFeature("privatebrowsing"),
    "Enrollment was validated and stored"
  );

  manager = ExperimentFakes.manager();
  await manager.onStartup();

  let experiment = ExperimentFakes.experiment("featuremanifest_enum_fail", {
    branch: {
      slug: "control",
      ratio: 1,
      feature: {
        featureId: "privatebrowsing",
        value: { promoLinkType: "bar" },
      },
    },
  });

  await Assert.rejects(
    manager.store.addExperiment(experiment),
    /promoLinkType should have one of the following values/,
    "This should fail because of invalid feature value"
  );

  Assert.ok(
    !manager.store.hasExperimentForFeature("privatebrowsing"),
    "experiment is not stored"
  );
});

add_task(async function test_featureIds_is_stored() {
  Services.prefs.setStringPref("messaging-system.log", "all");
  const recipe = ExperimentFakes.recipe("featureIds");
  // Ensure we get enrolled
  recipe.bucketConfig.count = recipe.bucketConfig.total;
  const manager = ExperimentFakes.manager();

  await manager.onStartup();

  await manager.enroll(recipe, "test_featureIds_is_stored");

  Assert.ok(manager.store.addExperiment.calledOnce, "experiment is stored");
  let [enrollment] = manager.store.addExperiment.firstCall.args;
  Assert.ok("featureIds" in enrollment, "featureIds is stored");
  Assert.deepEqual(
    enrollment.featureIds,
    ["test-feature"],
    "Has expected value"
  );
});
