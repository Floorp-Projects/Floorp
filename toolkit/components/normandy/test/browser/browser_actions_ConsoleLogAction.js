"use strict";

ChromeUtils.import("resource://normandy/actions/BaseAction.jsm", this);
ChromeUtils.import("resource://normandy/actions/ConsoleLogAction.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);

// Test that logging works
add_task(async function logging_works() {
  const action = new ConsoleLogAction();
  const infoStub = sinon.stub(action.log, "info");
  try {
    const recipe = { id: 1, arguments: { message: "Hello, world!" } };
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    Assert.deepEqual(
      infoStub.args,
      ["Hello, world!"],
      "the message should be logged"
    );
  } finally {
    infoStub.restore();
  }
});

// test that argument validation works
decorate_task(
  withStub(Uptake, "reportRecipe"),
  async function arguments_are_validated(reportRecipeStub) {
    const action = new ConsoleLogAction();
    const infoStub = sinon.stub(action.log, "info");

    try {
      // message is required
      let recipe = { id: 1, arguments: {} };
      await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
      is(action.lastError, null, "lastError should be null");
      Assert.deepEqual(infoStub.args, [], "no message should be logged");
      Assert.deepEqual(reportRecipeStub.args, [
        [recipe, Uptake.RECIPE_EXECUTION_ERROR],
      ]);

      reportRecipeStub.reset();

      // message must be a string
      recipe = { id: 1, arguments: { message: 1 } };
      await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
      is(action.lastError, null, "lastError should be null");
      Assert.deepEqual(infoStub.args, [], "no message should be logged");
      Assert.deepEqual(reportRecipeStub.args, [
        [recipe, Uptake.RECIPE_EXECUTION_ERROR],
      ]);
    } finally {
      infoStub.restore();
    }
  }
);
