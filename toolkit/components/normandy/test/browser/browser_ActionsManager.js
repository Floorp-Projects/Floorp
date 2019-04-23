"use strict";

ChromeUtils.import("resource://normandy/lib/ActionsManager.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyApi.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);

// Test life cycle methods for actions
decorate_task(
  async function(reportActionStub, Stub) {
    let manager = new ActionsManager();
    const recipe = {id: 1, action: "test-local-action-used"};

    let actionUsed = {
      runRecipe: sinon.stub(),
      finalize: sinon.stub(),
    };
    let actionUnused = {
      runRecipe: sinon.stub(),
      finalize: sinon.stub(),
    };
    manager.localActions = {
      "test-local-action-used": actionUsed,
      "test-local-action-unused": actionUnused,
    };

    await manager.runRecipe(recipe);
    await manager.finalize();

    Assert.deepEqual(actionUsed.runRecipe.args, [[recipe]], "used action should be called with the recipe");
    ok(actionUsed.finalize.calledOnce, "finalize should be called on used action");
    Assert.deepEqual(actionUnused.runRecipe.args, [], "unused action should not be called with the recipe");
    ok(actionUnused.finalize.calledOnce, "finalize should be called on the unused action");
  },
);

