"use strict";

ChromeUtils.import("resource://normandy/actions/BaseAction.jsm", this);
ChromeUtils.import("resource://normandy/lib/ActionsManager.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyApi.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);
const { ActionSchemas } = ChromeUtils.import(
  "resource://normandy/actions/schemas/index.js"
);

// Test life cycle methods for actions
decorate_task(async function(reportActionStub, Stub) {
  let manager = new ActionsManager();
  const recipe = { id: 1, action: "test-local-action-used" };

  let actionUsed = {
    processRecipe: sinon.stub(),
    finalize: sinon.stub(),
  };
  let actionUnused = {
    processRecipe: sinon.stub(),
    finalize: sinon.stub(),
  };
  manager.localActions = {
    "test-local-action-used": actionUsed,
    "test-local-action-unused": actionUnused,
  };

  await manager.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
  await manager.finalize();

  Assert.deepEqual(
    actionUsed.processRecipe.args,
    [[recipe, BaseAction.suitability.FILTER_MATCH]],
    "used action should be called with the recipe"
  );
  ok(
    actionUsed.finalize.calledOnce,
    "finalize should be called on used action"
  );
  Assert.deepEqual(
    actionUnused.processRecipe.args,
    [],
    "unused action should not be called with the recipe"
  );
  ok(
    actionUnused.finalize.calledOnce,
    "finalize should be called on the unused action"
  );
});

decorate_task(async function() {
  for (const [name, Constructor] of Object.entries(
    ActionsManager.actionConstructors
  )) {
    const action = new Constructor();
    Assert.deepEqual(
      ActionSchemas[name],
      action.schema,
      "action name should map to a schema"
    );
  }
});
