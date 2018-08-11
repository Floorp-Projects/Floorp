"use strict";

ChromeUtils.import("resource://normandy/actions/BaseAction.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);

class NoopAction extends BaseAction {
  _run(recipe) {
    // does nothing
  }
}

class FailPreExecutionAction extends BaseAction {
  constructor() {
    super();
    this._testRunFlag = false;
    this._testFinalizeFlag = false;
  }

  _preExecution() {
    throw new Error("Test error");
  }

  _run() {
    this._testRunFlag = true;
  }

  _finalize() {
    this._testFinalizeFlag = true;
  }
}

class FailRunAction extends BaseAction {
  constructor() {
    super();
    this._testRunFlag = false;
    this._testFinalizeFlag = false;
  }

  _run(recipe) {
    throw new Error("Test error");
  }

  _finalize() {
    this._testFinalizeFlag = true;
  }
}

class FailFinalizeAction extends BaseAction {
  _run(recipe) {
    // does nothing
  }

  _finalize() {
    throw new Error("Test error");
  }
}

let _recipeId = 1;
function recipeFactory(overrides) {
  let defaults = {
    id: _recipeId++,
    arguments: {},
  };
  Object.assign(defaults, overrides);
  return defaults;
}

// Test that per-recipe uptake telemetry is recorded
decorate_task(
  withStub(Uptake, "reportRecipe"),
  async function(reportRecipeStub) {
    const action = new NoopAction();
    const recipe = recipeFactory();
    await action.runRecipe(recipe);
    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe.id, Uptake.RECIPE_SUCCESS]],
      "per-recipe uptake telemetry should be reported",
    );
  },
);

// Finalize causes action telemetry to be recorded
decorate_task(
  withStub(Uptake, "reportAction"),
  async function(reportActionStub) {
    const action = new NoopAction();
    await action.finalize();
    ok(action.finalized, "Action should be marked as finalized");
    Assert.deepEqual(
      reportActionStub.args,
      [[action.name, Uptake.ACTION_SUCCESS]],
      "action uptake telemetry should be reported",
    );
  },
);

// Recipes can't be run after finalize is called
decorate_task(
  withStub(Uptake, "reportRecipe"),
  async function(reportRecipeStub) {
    const action = new NoopAction();
    const recipe1 = recipeFactory();
    const recipe2 = recipeFactory();

    await action.runRecipe(recipe1);
    await action.finalize();

    await Assert.rejects(
      action.runRecipe(recipe2),
      /^Error: Action has already been finalized$/,
      "running recipes after finalization is an error",
    );

    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe1.id, Uptake.RECIPE_SUCCESS]],
      "Only recipes executed prior to finalizer should report uptake telemetry",
    );
  },
);

// Test an action with a failing pre-execution step
decorate_task(
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  async function(reportRecipeStub, reportActionStub) {
    const recipe = recipeFactory();
    const action = new FailPreExecutionAction();
    ok(action.failed, "Action should fail during pre-execution fail");

    // Should not throw, even though the action is in a failed state.
    await action.runRecipe(recipe);

    // Should not throw, even though the action is in a failed state.
    await action.finalize();

    is(action._testRunFlag, false, "_run should not have been caled");
    is(action._testFinalizeFlag, false, "_finalize should not have been caled");

    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe.id, Uptake.RECIPE_ACTION_DISABLED]],
      "Recipe should report recipe status as action disabled",
    );

    Assert.deepEqual(
      reportActionStub.args,
      [[action.name, Uptake.ACTION_PRE_EXECUTION_ERROR]],
      "Action should report pre execution error",
    );
  },
);

// Test an action with a failing recipe step
decorate_task(
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  async function(reportRecipeStub, reportActionStub) {
    const recipe = recipeFactory();
    const action = new FailRunAction();
    await action.runRecipe(recipe);
    await action.finalize();
    ok(!action.failed, "Action should not be marked as failed due to a recipe failure");

    ok(action._testFinalizeFlag, "_finalize should have been called");

    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe.id, Uptake.RECIPE_EXECUTION_ERROR]],
      "Recipe should report recipe execution error",
    );

    Assert.deepEqual(
      reportActionStub.args,
      [[action.name, Uptake.ACTION_SUCCESS]],
      "Action should report success",
    );
  },
);

// Test an action with a failing finalize step
decorate_task(
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  async function(reportRecipeStub, reportActionStub) {
    const recipe = recipeFactory();
    const action = new FailFinalizeAction();
    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe.id, Uptake.RECIPE_SUCCESS]],
      "Recipe should report success",
    );

    Assert.deepEqual(
      reportActionStub.args,
      [[action.name, Uptake.ACTION_POST_EXECUTION_ERROR]],
      "Action should report post execution error",
    );
  },
);
