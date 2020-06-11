"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);

const { FirstStartup } = ChromeUtils.import(
  "resource://gre/modules/FirstStartup.jsm"
);

add_task(async function test_createTargetingContext() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const recipe = ExperimentFakes.recipe("foo");
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActive").returns([recipe]);

  let context = manager.createTargetingContext();
  const activeSlugs = await context.activeExperiments;

  Assert.ok(!context.isFirstStartup, "should not set the first startup flag");
  Assert.deepEqual(
    activeSlugs,
    ["foo"],
    "should return slugs for all the active experiment"
  );

  // Pretend to be in the first startup
  FirstStartup._state = FirstStartup.IN_PROGRESS;
  context = manager.createTargetingContext();

  Assert.ok(context.isFirstStartup, "should set the first startup flag");
});
