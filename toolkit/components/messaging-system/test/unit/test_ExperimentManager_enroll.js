"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
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
    groups: ["red", "pink"],
    value: { title: "hello" },
  };
  const newBranch = {
    slug: "treatment",
    groups: ["pink"],
    value: { title: "hi" },
  };

  // simulate adding an experiment with a conflicting group "pink"
  manager.store.addExperiment(
    ExperimentFakes.experiment("foo", {
      branch: existingBranch,
    })
  );

  // ensure .enroll chooses the special branch with the conflict
  sandbox.stub(manager, "chooseBranch").returns(newBranch);
  await Assert.rejects(
    manager.enroll(ExperimentFakes.recipe("bar", { branches: [newBranch] })),
    /An experiment with a conflicting group already exists/,
    "should throw if there is a group conflict"
  );

  Assert.equal(
    manager.sendFailureTelemetry.calledWith(
      "enrollFailed",
      "bar",
      "group-conflict"
    ),
    true,
    "should send failure telemetry if a group conflict exists"
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
