"use strict";

ChromeUtils.import("resource://normandy/lib/ActionsManager.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyApi.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);

// It should only fetch implementations for actions that don't exist locally
decorate_task(
  withStub(NormandyApi, "fetchActions"),
  withStub(NormandyApi, "fetchImplementation"),
  async function(fetchActionsStub, fetchImplementationStub) {
    const remoteAction = {name: "remote-action"};
    const localAction = {name: "local-action"};
    fetchActionsStub.resolves([remoteAction, localAction]);
    fetchImplementationStub.callsFake(async () => "");

    const manager = new ActionsManager();
    manager.localActions = {"local-action": {}};
    await manager.fetchRemoteActions();

    is(fetchActionsStub.callCount, 1, "action metadata should be fetched");
    Assert.deepEqual(
      fetchImplementationStub.getCall(0).args,
      [remoteAction],
      "only the remote action's implementation should be fetched",
    );
  },
);

// Test life cycle methods for remote actions
decorate_task(
  withStub(Uptake, "reportAction"),
  withStub(Uptake, "reportRecipe"),
  async function(reportActionStub, reportRecipeStub) {
    let manager = new ActionsManager();
    const recipe = {id: 1, action: "test-remote-action-used"};

    const sandboxManagerUsed = {
      removeHold: sinon.stub(),
      runAsyncCallback: sinon.stub(),
    };
    const sandboxManagerUnused = {
      removeHold: sinon.stub(),
      runAsyncCallback: sinon.stub(),
    };
    manager.remoteActionSandboxes = {
      "test-remote-action-used": sandboxManagerUsed,
      "test-remote-action-unused": sandboxManagerUnused,
    };
    manager.localActions = {};

    await manager.preExecution();
    await manager.runRecipe(recipe);
    await manager.finalize();

    Assert.deepEqual(
      sandboxManagerUsed.runAsyncCallback.args,
      [
        ["preExecution"],
        ["action", recipe],
        ["postExecution"],
      ],
      "The expected life cycle events should be called on the used sandbox action manager",
    );
    Assert.deepEqual(
      sandboxManagerUnused.runAsyncCallback.args,
      [
        ["preExecution"],
        ["postExecution"],
      ],
      "The expected life cycle events should be called on the unused sandbox action manager",
    );
    Assert.deepEqual(
      sandboxManagerUsed.removeHold.args,
      [["ActionsManager"]],
      "ActionsManager should remove holds on the sandbox managers during finalize.",
    );
    Assert.deepEqual(
      sandboxManagerUnused.removeHold.args,
      [["ActionsManager"]],
      "ActionsManager should remove holds on the sandbox managers during finalize.",
    );

    Assert.deepEqual(reportActionStub.args, [
      ["test-remote-action-used", Uptake.ACTION_SUCCESS],
      ["test-remote-action-unused", Uptake.ACTION_SUCCESS],
    ]);
    Assert.deepEqual(reportRecipeStub.args, [[recipe.id, Uptake.RECIPE_SUCCESS]]);
  },
);

// Test life cycle for remote action that fails in pre-step
decorate_task(
  withStub(Uptake, "reportAction"),
  withStub(Uptake, "reportRecipe"),
  async function(reportActionStub, reportRecipeStub) {
    let manager = new ActionsManager();
    const recipe = {id: 1, action: "test-remote-action-broken"};

    const sandboxManagerBroken = {
      removeHold: sinon.stub(),
      runAsyncCallback: sinon.stub().callsFake(callbackName => {
        if (callbackName === "preExecution") {
          throw new Error("mock preExecution failure");
        }
      }),
    };
    manager.remoteActionSandboxes = {
      "test-remote-action-broken": sandboxManagerBroken,
    };
    manager.localActions = {};

    await manager.preExecution();
    await manager.runRecipe(recipe);
    await manager.finalize();

    Assert.deepEqual(
      sandboxManagerBroken.runAsyncCallback.args,
      [["preExecution"]],
      "No async callbacks should be called after preExecution fails",
    );
    Assert.deepEqual(
      sandboxManagerBroken.removeHold.args,
      [["ActionsManager"]],
      "sandbox holds should still be removed after a failure",
    );

    Assert.deepEqual(reportActionStub.args, [
      ["test-remote-action-broken", Uptake.ACTION_PRE_EXECUTION_ERROR],
    ]);
    Assert.deepEqual(reportRecipeStub.args, [[recipe.id, Uptake.RECIPE_ACTION_DISABLED]]);
  },
);

// Test life cycle for remote action that fails on a recipe-step
decorate_task(
  withStub(Uptake, "reportAction"),
  withStub(Uptake, "reportRecipe"),
  async function(reportActionStub, reportRecipeStub) {
    let manager = new ActionsManager();
    const recipe = {id: 1, action: "test-remote-action-broken"};

    const sandboxManagerBroken = {
      removeHold: sinon.stub(),
      runAsyncCallback: sinon.stub().callsFake(callbackName => {
        if (callbackName === "action") {
          throw new Error("mock action failure");
        }
      }),
    };
    manager.remoteActionSandboxes = {
      "test-remote-action-broken": sandboxManagerBroken,
    };
    manager.localActions = {};

    await manager.preExecution();
    await manager.runRecipe(recipe);
    await manager.finalize();

    Assert.deepEqual(
      sandboxManagerBroken.runAsyncCallback.args,
      [["preExecution"], ["action", recipe], ["postExecution"]],
      "postExecution callback should still be called after action callback fails",
    );
    Assert.deepEqual(
      sandboxManagerBroken.removeHold.args,
      [["ActionsManager"]],
      "sandbox holds should still be removed after a recipe failure",
    );

    Assert.deepEqual(reportActionStub.args, [["test-remote-action-broken", Uptake.ACTION_SUCCESS]]);
    Assert.deepEqual(reportRecipeStub.args, [[recipe.id, Uptake.RECIPE_EXECUTION_ERROR]]);
  },
);

// Test life cycle for remote action that fails in post-step
decorate_task(
  withStub(Uptake, "reportAction"),
  withStub(Uptake, "reportRecipe"),
  async function(reportActionStub, reportRecipeStub) {
    let manager = new ActionsManager();
    const recipe = {id: 1, action: "test-remote-action-broken"};

    const sandboxManagerBroken = {
      removeHold: sinon.stub(),
      runAsyncCallback: sinon.stub().callsFake(callbackName => {
        if (callbackName === "postExecution") {
          throw new Error("mock postExecution failure");
        }
      }),
    };
    manager.remoteActionSandboxes = {
      "test-remote-action-broken": sandboxManagerBroken,
    };
    manager.localActions = {};

    await manager.preExecution();
    await manager.runRecipe(recipe);
    await manager.finalize();

    Assert.deepEqual(
      sandboxManagerBroken.runAsyncCallback.args,
      [["preExecution"], ["action", recipe], ["postExecution"]],
      "All callbacks should be executed",
    );
    Assert.deepEqual(
      sandboxManagerBroken.removeHold.args,
      [["ActionsManager"]],
      "sandbox holds should still be removed after a failure",
    );

    Assert.deepEqual(reportRecipeStub.args, [[recipe.id, Uptake.RECIPE_SUCCESS]]);
    Assert.deepEqual(reportActionStub.args, [
      ["test-remote-action-broken", Uptake.ACTION_POST_EXECUTION_ERROR],
    ]);
  },
);

// Test life cycle methods for local actions
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
    manager.remoteActionSandboxes = {};

    await manager.preExecution();
    await manager.runRecipe(recipe);
    await manager.finalize();

    Assert.deepEqual(actionUsed.runRecipe.args, [[recipe]], "used action should be called with the recipe");
    ok(actionUsed.finalize.calledOnce, "finalize should be called on used action");
    Assert.deepEqual(actionUnused.runRecipe.args, [], "unused action should not be called with the recipe");
    ok(actionUnused.finalize.calledOnce, "finalize should be called on the unused action");

    // Uptake telemetry is handled by actions directly, so doesn't
    // need to be tested for local action handling here.
  },
);

// Likewise, error handling is dealt with internal to actions as well,
// so doesn't need to be tested as a part of ActionsManager.

// Test fetch remote actions
decorate_task(
  withStub(NormandyApi, "fetchActions"),
  withStub(NormandyApi, "fetchImplementation"),
  withStub(Uptake, "reportAction"),
  async function(fetchActionsStub, fetchImplementationStub, reportActionStub) {
    fetchActionsStub.callsFake(async () => [
      {name: "remoteAction"},
      {name: "missingImpl"},
      {name: "migratedAction"},
    ]);
    fetchImplementationStub.callsFake(async ({ name }) => {
      switch (name) {
        case "remoteAction":
          return "window.scriptRan = true";
        case "missingImpl":
          throw new Error(`Could not fetch implementation for ${name}: test error`);
        case "migratedAction":
          return "// this shouldn't be requested";
        default:
          throw new Error(`Could not fetch implementation for ${name}: unexpected action`);
      }
    });

    const manager = new ActionsManager();
    manager.localActions = {
      migratedAction: {finalize: sinon.stub()},
    };

    await manager.fetchRemoteActions();

    Assert.deepEqual(
      Object.keys(manager.remoteActionSandboxes),
      ["remoteAction"],
      "remote action should have been loaded",
    );

    Assert.deepEqual(
      fetchImplementationStub.args,
      [[{name: "remoteAction"}], [{name: "missingImpl"}]],
      "all remote actions should be requested",
    );

    Assert.deepEqual(
      reportActionStub.args,
      [["missingImpl", Uptake.ACTION_SERVER_ERROR]],
      "Missing implementation should be reported via Uptake",
    );

    ok(
      await manager.remoteActionSandboxes.remoteAction.evalInSandbox("window.scriptRan"),
      "Implementations should be run in the sandbox",
    );

    // clean up sandboxes made by fetchRemoteActions
    manager.finalize();
  },
);
