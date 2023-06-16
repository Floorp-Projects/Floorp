"use strict";

const { BaseAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/BaseAction.sys.mjs"
);
const { ActionsManager } = ChromeUtils.importESModule(
  "resource://normandy/lib/ActionsManager.sys.mjs"
);
const { Uptake } = ChromeUtils.importESModule(
  "resource://normandy/lib/Uptake.sys.mjs"
);
const { ActionSchemas } = ChromeUtils.importESModule(
  "resource://normandy/actions/schemas/index.sys.mjs"
);

// Test life cycle methods for actions
decorate_task(async function (reportActionStub, Stub) {
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

decorate_task(async function () {
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
