"use strict";

const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
const { Uptake } = ChromeUtils.import("resource://normandy/lib/Uptake.jsm");

class NoopAction extends BaseAction {
  constructor() {
    super();
    this._testPreExecutionFlag = false;
    this._testRunFlag = false;
    this._testFinalizeFlag = false;
  }

  _preExecution() {
    this._testPreExecutionFlag = true;
  }

  _run(recipe) {
    this._testRunFlag = true;
  }

  _finalize() {
    this._testFinalizeFlag = true;
  }
}

NoopAction._errorToThrow = new Error("test error");

class FailPreExecutionAction extends NoopAction {
  _preExecution() {
    throw NoopAction._errorToThrow;
  }
}

class FailRunAction extends NoopAction {
  _run(recipe) {
    throw NoopAction._errorToThrow;
  }
}

class FailFinalizeAction extends NoopAction {
  _finalize() {
    throw NoopAction._errorToThrow;
  }
}

// Test that constructor and override methods are run
decorate_task(
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  async () => {
    let action = new NoopAction();
    is(
      action._testPreExecutionFlag,
      false,
      "_preExecution should not have been called on a new action"
    );
    is(
      action._testRunFlag,
      false,
      "_run has should not have been called on a new action"
    );
    is(
      action._testFinalizeFlag,
      false,
      "_finalize should not be called on a new action"
    );

    const recipe = recipeFactory();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(
      action._testPreExecutionFlag,
      true,
      "_preExecution should be called when a recipe is executed"
    );
    is(
      action._testRunFlag,
      true,
      "_run should be called when a recipe is executed"
    );
    is(
      action._testFinalizeFlag,
      false,
      "_finalize should not have been called when a recipe is executed"
    );

    await action.finalize();
    is(
      action._testFinalizeFlag,
      true,
      "_finalizeExecution should be called when finalize was called"
    );

    action = new NoopAction();
    await action.finalize();
    is(
      action._testPreExecutionFlag,
      true,
      "_preExecution should be called when finalized even if no recipes"
    );
    is(
      action._testRunFlag,
      false,
      "_run should be called if no recipes were run"
    );
    is(
      action._testFinalizeFlag,
      true,
      "_finalize should be called when finalized"
    );
  }
);

// Test that per-recipe uptake telemetry is recorded
decorate_task(withStub(Uptake, "reportRecipe"), async function({
  reportRecipeStub,
}) {
  const action = new NoopAction();
  const recipe = recipeFactory();
  await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
  Assert.deepEqual(
    reportRecipeStub.args,
    [[recipe, Uptake.RECIPE_SUCCESS]],
    "per-recipe uptake telemetry should be reported"
  );
});

// Finalize causes action telemetry to be recorded
decorate_task(withStub(Uptake, "reportAction"), async function({
  reportActionStub,
}) {
  const action = new NoopAction();
  await action.finalize();
  ok(
    action.state == NoopAction.STATE_FINALIZED,
    "Action should be marked as finalized"
  );
  Assert.deepEqual(
    reportActionStub.args,
    [[action.name, Uptake.ACTION_SUCCESS]],
    "action uptake telemetry should be reported"
  );
});

// Recipes can't be run after finalize is called
decorate_task(withStub(Uptake, "reportRecipe"), async function({
  reportRecipeStub,
}) {
  const action = new NoopAction();
  const recipe1 = recipeFactory();
  const recipe2 = recipeFactory();

  await action.processRecipe(recipe1, BaseAction.suitability.FILTER_MATCH);
  await action.finalize();

  await Assert.rejects(
    action.processRecipe(recipe2, BaseAction.suitability.FILTER_MATCH),
    /^Error: Action has already been finalized$/,
    "running recipes after finalization is an error"
  );

  Assert.deepEqual(
    reportRecipeStub.args,
    [[recipe1, Uptake.RECIPE_SUCCESS]],
    "Only recipes executed prior to finalizer should report uptake telemetry"
  );
});

// Test an action with a failing pre-execution step
decorate_task(
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  async function({ reportRecipeStub, reportActionStub }) {
    const recipe = recipeFactory();
    const action = new FailPreExecutionAction();
    is(
      action.state,
      FailPreExecutionAction.STATE_PREPARING,
      "Pre-execution should not happen immediately"
    );

    // Should fail, putting the action into a "failed" state, but the entry
    // point `processRecipe` should not itself throw an exception.
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(
      action.state,
      FailPreExecutionAction.STATE_FAILED,
      "Action fails if pre-execution fails"
    );
    is(
      action.lastError,
      NoopAction._errorToThrow,
      "The thrown error should be stored in lastError"
    );

    // Should not throw, even though the action is in a disabled state.
    await action.finalize();
    is(
      action.state,
      FailPreExecutionAction.STATE_FINALIZED,
      "Action should be finalized"
    );
    is(
      action.lastError,
      NoopAction._errorToThrow,
      "lastError should not have changed"
    );

    is(action._testRunFlag, false, "_run should not have been called");
    is(
      action._testFinalizeFlag,
      false,
      "_finalize should not have been called"
    );

    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe, Uptake.RECIPE_ACTION_DISABLED]],
      "Recipe should report recipe status as action disabled"
    );

    Assert.deepEqual(
      reportActionStub.args,
      [[action.name, Uptake.ACTION_PRE_EXECUTION_ERROR]],
      "Action should report pre execution error"
    );
  }
);

// Test an action with a failing recipe step
decorate_task(
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  async function({ reportRecipeStub, reportActionStub }) {
    const recipe = recipeFactory();
    const action = new FailRunAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(
      action.state,
      FailRunAction.STATE_READY,
      "Action should not be marked as failed due to a recipe failure"
    );
    await action.finalize();
    is(
      action.state,
      FailRunAction.STATE_FINALIZED,
      "Action should be marked as finalized after finalize is called"
    );

    ok(action._testFinalizeFlag, "_finalize should have been called");

    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe, Uptake.RECIPE_EXECUTION_ERROR]],
      "Recipe should report recipe execution error"
    );

    Assert.deepEqual(
      reportActionStub.args,
      [[action.name, Uptake.ACTION_SUCCESS]],
      "Action should report success"
    );
  }
);

// Test an action with a failing finalize step
decorate_task(
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  async function({ reportRecipeStub, reportActionStub }) {
    const recipe = recipeFactory();
    const action = new FailFinalizeAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();

    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe, Uptake.RECIPE_SUCCESS]],
      "Recipe should report success"
    );

    Assert.deepEqual(
      reportActionStub.args,
      [[action.name, Uptake.ACTION_POST_EXECUTION_ERROR]],
      "Action should report post execution error"
    );
  }
);

// Disable disables an action
decorate_task(
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  async function({ reportRecipeStub, reportActionStub }) {
    const recipe = recipeFactory();
    const action = new NoopAction();

    action.disable();
    is(
      action.state,
      NoopAction.STATE_DISABLED,
      "Action should be marked as disabled"
    );

    // Should not throw, even though the action is disabled
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);

    // Should not throw, even though the action is disabled
    await action.finalize();

    is(action._testRunFlag, false, "_run should not have been called");
    is(
      action._testFinalizeFlag,
      false,
      "_finalize should not have been called"
    );

    Assert.deepEqual(
      reportActionStub.args,
      [[action.name, Uptake.ACTION_SUCCESS]],
      "Action should not report pre execution error"
    );

    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe, Uptake.RECIPE_ACTION_DISABLED]],
      "Recipe should report recipe status as action disabled"
    );
  }
);

// If the capabilities don't match, processRecipe shouldn't validate the arguments
decorate_task(async function() {
  const recipe = recipeFactory();
  const action = new NoopAction();
  const verifySpy = sinon.spy(action, "validateArguments");
  await action.processRecipe(
    recipe,
    BaseAction.suitability.CAPABILITES_MISMATCH
  );
  ok(!verifySpy.called, "validateArguments should not be called");
});
