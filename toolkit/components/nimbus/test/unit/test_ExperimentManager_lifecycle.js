"use strict";

const { Sampling } = ChromeUtils.import(
  "resource://gre/modules/components-utils/Sampling.jsm"
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

add_task(async function test_onStartup_setRolloutActive_called() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.stub(manager, "setExperimentActive");
  sandbox.stub(manager.store, "init").resolves();

  const active = ["foo", "bar"].map(ExperimentFakes.rollout);
  sandbox.stub(manager.store, "getAll").returns(active);

  await manager.onStartup();

  active.forEach(r =>
    Assert.equal(
      manager.setExperimentActive.calledWith(r),
      true,
      `should call setExperimentActive for rollout: ${r.slug}`
    )
  );
});

add_task(async function test_startup_unenroll() {
  Services.prefs.setBoolPref("app.shield.optoutstudies.enabled", false);
  const store = ExperimentFakes.store();
  const sandbox = sinon.createSandbox();
  let recipe = ExperimentFakes.experiment("startup_unenroll", {
    experimentType: "unittest",
    source: "test",
  });
  // Test initializing ExperimentManager with an active
  // recipe in the store. If the user has opted out it should
  // unenroll.
  await store.init();
  let enrollmentPromise = new Promise(resolve =>
    store.on(`update:${recipe.slug}`, resolve)
  );
  store.addEnrollment(recipe);
  await enrollmentPromise;

  const manager = ExperimentFakes.manager(store);
  const unenrollStub = sandbox.stub(manager, "unenroll");

  await manager.onStartup();

  Assert.ok(
    unenrollStub.calledOnce,
    "Unenrolled from active experiment if user opt out is true"
  );
  Assert.ok(
    unenrollStub.calledWith("startup_unenroll", "studies-opt-out"),
    "Called unenroll for expected recipe"
  );

  Services.prefs.clearUserPref("app.shield.optoutstudies.enabled");
});

/**
 * onRecipe()
 * - should add recipe slug to .session[source]
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
  await manager.onRecipe(fooRecipe, "test");

  Assert.equal(
    manager.sessions.get("test").has("foo"),
    true,
    "should add slug to sessions[test]"
  );
});

add_task(async function test_onRecipe_enroll() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.stub(manager, "isInBucketAllocation").resolves(true);
  sandbox.stub(Sampling, "bucketSample").resolves(true);
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const fooRecipe = ExperimentFakes.recipe("foo");
  const experimentUpdate = new Promise(resolve =>
    manager.store.on(`update:${fooRecipe.slug}`, resolve)
  );
  await manager.onStartup();
  await manager.onRecipe(fooRecipe, "test");

  Assert.equal(
    manager.enroll.calledWith(fooRecipe),
    true,
    "should call .enroll() the first time a recipe is seen"
  );
  await experimentUpdate;
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
  sandbox.stub(manager, "isInBucketAllocation").resolves(true);

  const fooRecipe = ExperimentFakes.recipe("foo");
  const experimentUpdate = new Promise(resolve =>
    manager.store.on(`update:${fooRecipe.slug}`, resolve)
  );

  await manager.onStartup();
  await manager.onRecipe(fooRecipe, "test");
  // onRecipe calls enroll which saves the experiment in the store
  // but none of them wait on disk operations to finish
  await experimentUpdate;
  // Call again after recipe has already been enrolled
  await manager.onRecipe(fooRecipe, "test");

  Assert.equal(
    manager.updateEnrollment.calledWith(fooRecipe),
    true,
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
});

add_task(async function test_onRecipe_rollout_update() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "unenroll");
  sandbox.spy(manager, "updateEnrollment");
  sandbox.stub(manager, "isInBucketAllocation").resolves(true);

  const fooRecipe = {
    ...ExperimentFakes.recipe("foo"),
    isRollout: true,
  };
  // Rollouts should only have 1 branch
  fooRecipe.branches = fooRecipe.branches.slice(0, 1);
  const experimentUpdate = new Promise(resolve =>
    manager.store.on(`update:${fooRecipe.slug}`, resolve)
  );

  await manager.onStartup();
  await manager.onRecipe(fooRecipe, "test");
  // onRecipe calls enroll which saves the experiment in the store
  // but none of them wait on disk operations to finish
  await experimentUpdate;
  // Call again after recipe has already been enrolled
  await manager.onRecipe(fooRecipe, "test");

  Assert.equal(
    manager.updateEnrollment.calledWith(fooRecipe),
    true,
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
  Assert.ok(
    manager.updateEnrollment.alwaysReturned(true),
    "updateEnrollment will confirm the enrolled branch still exists in the recipe and exit"
  );
  Assert.ok(
    manager.unenroll.notCalled,
    "Should not call if the branches did not change"
  );

  // We call again but this time we change the branch slug
  // Has to be a deep clone otherwise you're changing the
  // value found in the experiment store
  let recipeClone = Cu.cloneInto(fooRecipe, {});
  recipeClone.branches[0].slug = "control-v2";
  await manager.onRecipe(recipeClone, "test");

  Assert.equal(
    manager.updateEnrollment.calledWith(recipeClone),
    true,
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
  Assert.ok(
    manager.unenroll.called,
    "updateEnrollment will unenroll because the branch slug changed"
  );
  Assert.ok(
    manager.unenroll.calledWith(fooRecipe.slug, "branch-removed"),
    "updateEnrollment will unenroll because the branch slug changed"
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
  await manager.onRecipe(pausedRecipe, "test");
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
  let enrollmentPromise = new Promise(resolve =>
    manager.store.on(`update:${fooRecipe.slug}`, resolve)
  );
  await manager.enroll(fooRecipe, "test");
  await enrollmentPromise;
  await manager.onRecipe(updatedRecipe, "test");
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
  let recipe0 = ExperimentFakes.experiment("foo", {
    experimentType: "unittest",
    userFacingName: "foo",
    userFacingDescription: "foo",
    lastSeen: Date.now().toLocaleString(),
    source: "test",
  });
  await manager.store.addEnrollment(recipe0);

  const recipe1 = ExperimentFakes.recipe("bar");
  // Unique features to prevent overlap
  recipe1.branches[0].features[0].featureId = "red";
  recipe1.branches[1].features[0].featureId = "red";
  await manager.onRecipe(recipe1, "test");
  const recipe2 = ExperimentFakes.recipe("baz");
  recipe2.branches[0].features[0].featureId = "green";
  recipe2.branches[1].features[0].featureId = "green";
  await manager.onRecipe(recipe2, "test");

  // Finalize
  manager.onFinalize("test");

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
    manager.sessions.has("test"),
    false,
    "should clear sessions[test]"
  );
});

add_task(async function test_onFinalize_unenroll_mismatch() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "unenroll");

  await manager.onStartup();

  // Add an experiment to the store without calling .onRecipe
  // This simulates an enrollment having happened in the past.
  let recipe0 = ExperimentFakes.experiment("foo", {
    experimentType: "unittest",
    userFacingName: "foo",
    userFacingDescription: "foo",
    lastSeen: Date.now().toLocaleString(),
    source: "test",
  });
  await manager.store.addEnrollment(recipe0);

  const recipe1 = ExperimentFakes.recipe("bar");
  // Unique features to prevent overlap
  recipe1.branches[0].features[0].featureId = "red";
  recipe1.branches[1].features[0].featureId = "red";
  await manager.onRecipe(recipe1, "test");
  const recipe2 = ExperimentFakes.recipe("baz");
  recipe2.branches[0].features[0].featureId = "green";
  recipe2.branches[1].features[0].featureId = "green";
  await manager.onRecipe(recipe2, "test");

  // Finalize
  manager.onFinalize("test", { recipeMismatches: [recipe0.slug] });

  Assert.equal(
    manager.unenroll.callCount,
    1,
    "should only call unenroll for the unseen recipe"
  );
  Assert.equal(
    manager.unenroll.calledWith("foo", "targeting-mismatch"),
    true,
    "should unenroll a experiment whose recipe wasn't seen in the current session"
  );
  Assert.equal(
    manager.sessions.has("test"),
    false,
    "should clear sessions[test]"
  );
});

add_task(async function test_onFinalize_rollout_unenroll() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "unenroll");

  await manager.onStartup();

  let rollout = ExperimentFakes.rollout("rollout");
  await manager.store.addEnrollment(rollout);

  manager.onFinalize("NimbusTestUtils");

  Assert.equal(
    manager.unenroll.callCount,
    1,
    "should only call unenroll for the unseen recipe"
  );
  Assert.equal(
    manager.unenroll.calledWith("rollout", "recipe-not-seen"),
    true,
    "should unenroll a experiment whose recipe wasn't seen in the current session"
  );
});
