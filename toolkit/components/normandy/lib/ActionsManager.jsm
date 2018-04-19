ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://normandy/lib/LogManager.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ActionSandboxManager: "resource://normandy/lib/ActionSandboxManager.jsm",
  NormandyApi: "resource://normandy/lib/NormandyApi.jsm",
  Uptake: "resource://normandy/lib/Uptake.jsm",
  ConsoleLogAction: "resource://normandy/actions/ConsoleLogAction.jsm",
  PreferenceRolloutAction: "resource://normandy/actions/PreferenceRolloutAction.jsm",
  PreferenceRollbackAction: "resource://normandy/actions/PreferenceRollbackAction.jsm",
});

var EXPORTED_SYMBOLS = ["ActionsManager"];

const log = LogManager.getLogger("recipe-runner");

/**
 * A class to manage the actions that recipes can use in Normandy.
 *
 * This includes both remote and local actions. Remote actions
 * implementations are fetched from the Normandy server; their
 * lifecycles are managed by `normandy/lib/ActionSandboxManager.jsm`.
 * Local actions have their implementations packaged in the Normandy
 * client, and manage their lifecycles internally.
 */
class ActionsManager {
  constructor() {
    this.finalized = false;
    this.remoteActionSandboxes = {};

    this.localActions = {
      "console-log": new ConsoleLogAction(),
      "preference-rollout": new PreferenceRolloutAction(),
      "preference-rollback": new PreferenceRollbackAction(),
    };
  }

  async fetchRemoteActions() {
    const actions = await NormandyApi.fetchActions();

    for (const action of actions) {
      // Skip actions with local implementations
      if (action.name in this.localActions) {
        continue;
      }

      try {
        const implementation = await NormandyApi.fetchImplementation(action);
        const sandbox = new ActionSandboxManager(implementation);
        sandbox.addHold("ActionsManager");
        this.remoteActionSandboxes[action.name] = sandbox;
      } catch (err) {
        log.warn(`Could not fetch implementation for ${action.name}: ${err}`);

        let status;
        if (/NetworkError/.test(err)) {
          status = Uptake.ACTION_NETWORK_ERROR;
        } else {
          status = Uptake.ACTION_SERVER_ERROR;
        }
        Uptake.reportAction(action.name, status);
      }
    }

    const actionNames = Object.keys(this.remoteActionSandboxes);
    log.debug(`Fetched ${actionNames.length} actions from the server: ${actionNames.join(", ")}`);
  }

  async preExecution() {
    // Local actions run pre-execution hooks implicitly

    for (const [actionName, manager] of Object.entries(this.remoteActionSandboxes)) {
      try {
        await manager.runAsyncCallback("preExecution");
        manager.disabled = false;
      } catch (err) {
        log.error(`Could not run pre-execution hook for ${actionName}:`, err.message);
        manager.disabled = true;
        Uptake.reportAction(actionName, Uptake.ACTION_PRE_EXECUTION_ERROR);
      }
    }
  }

  async runRecipe(recipe) {
    let actionName = recipe.action;

    if (actionName in this.localActions) {
      log.info(`Executing recipe "${recipe.name}" (action=${recipe.action})`);
      const action = this.localActions[actionName];
      await action.runRecipe(recipe);

    } else if (actionName in this.remoteActionSandboxes) {
      let status;
      const manager = this.remoteActionSandboxes[recipe.action];

      if (manager.disabled) {
        log.warn(
          `Skipping recipe ${recipe.name} because ${recipe.action} failed during pre-execution.`
        );
        status = Uptake.RECIPE_ACTION_DISABLED;
      } else {
        try {
          log.info(`Executing recipe "${recipe.name}" (action=${recipe.action})`);
          await manager.runAsyncCallback("action", recipe);
          status = Uptake.RECIPE_SUCCESS;
        } catch (e) {
          e.message = `Could not execute recipe ${recipe.name}: ${e.message}`;
          Cu.reportError(e);
          status = Uptake.RECIPE_EXECUTION_ERROR;
        }
      }
      Uptake.reportRecipe(recipe.id, status);

    } else {
      log.error(
        `Could not execute recipe ${recipe.name}:`,
        `Action ${recipe.action} is either missing or invalid.`
      );
      Uptake.reportRecipe(recipe.id, Uptake.RECIPE_INVALID_ACTION);
    }
  }

  async finalize() {
    if (this.finalized) {
      throw new Error("ActionsManager has already been finalized");
    }
    this.finalized = true;

    // Finalize local actions
    for (const action of Object.values(this.localActions)) {
      action.finalize();
    }

    // Run post-execution hooks for remote actions
    for (const [actionName, manager] of Object.entries(this.remoteActionSandboxes)) {
      // Skip if pre-execution failed.
      if (manager.disabled) {
        log.info(`Skipping post-execution hook for ${actionName} due to earlier failure.`);
        continue;
      }

      try {
        await manager.runAsyncCallback("postExecution");
        Uptake.reportAction(actionName, Uptake.ACTION_SUCCESS);
      } catch (err) {
        log.info(`Could not run post-execution hook for ${actionName}:`, err.message);
        Uptake.reportAction(actionName, Uptake.ACTION_POST_EXECUTION_ERROR);
      }
    }

    // Nuke sandboxes
    Object.values(this.remoteActionSandboxes)
      .forEach(manager => manager.removeHold("ActionsManager"));
  }
}
