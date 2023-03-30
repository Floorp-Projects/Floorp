"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const { FirstStartup } = ChromeUtils.importESModule(
  "resource://gre/modules/FirstStartup.sys.mjs"
);

add_task(async function test_createTargetingContext() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const recipe = ExperimentFakes.recipe("foo");
  const rollout = ExperimentFakes.rollout("bar");
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActiveExperiments").returns([recipe]);
  sandbox.stub(manager.store, "getAllActiveRollouts").returns([rollout]);

  let context = manager.createTargetingContext();
  const activeSlugs = await context.activeExperiments;
  const activeRollouts = await context.activeRollouts;

  Assert.ok(!context.isFirstStartup, "should not set the first startup flag");
  Assert.deepEqual(
    activeSlugs,
    ["foo"],
    "should return slugs for all the active experiment"
  );
  Assert.deepEqual(
    activeRollouts,
    ["bar"],
    "should return slugs for all rollouts stored"
  );

  // Pretend to be in the first startup
  FirstStartup._state = FirstStartup.IN_PROGRESS;
  context = manager.createTargetingContext();

  Assert.ok(context.isFirstStartup, "should set the first startup flag");
});
