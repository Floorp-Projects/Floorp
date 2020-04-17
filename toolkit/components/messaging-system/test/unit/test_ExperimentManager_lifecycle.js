"use strict";

const { _ExperimentManager } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentManager.jsm"
);
const { ExperimentStore } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentStore.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);

/**
 * onStartup()
 * - should set call setExperimentActive for each active experiment
 */
add_task(async function test_onStartup_setExperimentActive_called() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const experiments = [];
  sandbox.stub(manager, "setExperimentActive");
  sandbox.stub(manager.store, "init").resolves();
  sandbox.stub(manager.store, "getAll").returns(experiments);

  const active = ["foo", "bar"].map(ExperimentFakes.experiment);

  const inactive = ["baz", "qux"].map(slug =>
    ExperimentFakes.experiment(slug, { active: false })
  );

  [...active, ...inactive].forEach(exp => experiments.push(exp));

  await manager.onStartup();

  active.forEach(exp =>
    Assert.equal(
      manager.setExperimentActive.calledWith(exp),
      true,
      `should call setExperimentActive for active experiment: ${exp.slug}`
    )
  );

  inactive.forEach(exp =>
    Assert.equal(
      manager.setExperimentActive.calledWith(exp),
      false,
      `should not call setExperimentActive for inactive experiment: ${exp.slug}`
    )
  );
});

/**
 * onRecipe()
 * - should add recipe slug to .slugsSeenInThisSession
 * - should call .enroll() if the recipe hasn't been seen before;
 * - should call .update() if the Enrollment already exists in the store;
 * - should skip enrollment if recipe.isEnrollmentPaused is true
 */
add_task(async function test_onRecipe_track_slug() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const fooRecipe = ExperimentFakes.recipe("foo");

  await manager.onStartup();
  // The first time a recipe has seen;
  await manager.onRecipe(fooRecipe);

  Assert.equal(
    manager.slugsSeenInThisSession.has("foo"),
    true,
    "should add slug to slugsSeenInThisSession"
  );
});

add_task(async function test_onRecipe_enroll() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const fooRecipe = ExperimentFakes.recipe("foo");

  await manager.onStartup();
  await manager.onRecipe(fooRecipe);

  Assert.equal(
    manager.enroll.calledWith(fooRecipe),
    true,
    "should call .enroll() the first time a recipe is seen"
  );
  Assert.equal(
    manager.store.has("foo"),
    true,
    "should add recipe to the store"
  );
});

add_task(async function test_onRecipe_update() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const fooRecipe = ExperimentFakes.recipe("foo");

  await manager.onStartup();

  await manager.onRecipe(fooRecipe);
  // Call again after recipe has already been enrolled
  await manager.onRecipe(fooRecipe);

  Assert.equal(
    manager.updateEnrollment.calledWith(fooRecipe),
    true,
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
});

add_task(async function test_onRecipe_isEnrollmentPaused() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  await manager.onStartup();

  const pausedRecipe = ExperimentFakes.recipe("xyz", {
    isEnrollmentPaused: true,
  });
  await manager.onRecipe(pausedRecipe);
  Assert.equal(
    manager.enroll.calledWith(pausedRecipe),
    false,
    "should skip enrollment for recipes that are paused"
  );
  Assert.equal(
    manager.store.has("xyz"),
    false,
    "should not add recipe to the store"
  );

  const fooRecipe = ExperimentFakes.recipe("foo");
  const updatedRecipe = ExperimentFakes.recipe("foo", {
    isEnrollmentPaused: true,
  });
  await manager.enroll(fooRecipe);
  await manager.onRecipe(updatedRecipe);
  Assert.equal(
    manager.updateEnrollment.calledWith(updatedRecipe),
    true,
    "should still update existing recipes, even if enrollment is paused"
  );
});

/**
 * onFinalize()
 * - should unenroll experiments that weren't seen in the current session
 */

add_task(async function test_onFinalize_unenroll() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "unenroll");

  await manager.onStartup();

  // Add an experiment to the store without calling .onRecipe
  // This simulates an enrollment having happened in the past.
  manager.store.addExperiment(ExperimentFakes.experiment("foo"));

  // Simulate adding some other recipes
  await manager.onRecipe(ExperimentFakes.recipe("bar"));
  await manager.onRecipe(ExperimentFakes.recipe("baz"));

  // Finalize
  manager.onFinalize();

  Assert.equal(
    manager.unenroll.callCount,
    1,
    "should only call unenroll for the unseen recipe"
  );
  Assert.equal(
    manager.unenroll.calledWith("foo", "recipe-not-seen"),
    true,
    "should unenroll a experiment whose recipe wasn't seen in the current session"
  );
  Assert.equal(
    manager.slugsSeenInThisSession.size,
    0,
    "should clear slugsSeenInThisSession"
  );
});
