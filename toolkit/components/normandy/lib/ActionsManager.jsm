const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {LogManager} = ChromeUtils.import("resource://normandy/lib/LogManager.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonStudyAction: "resource://normandy/actions/AddonStudyAction.jsm",
  ConsoleLogAction: "resource://normandy/actions/ConsoleLogAction.jsm",
  PreferenceExperimentAction: "resource://normandy/actions/PreferenceExperimentAction.jsm",
  PreferenceRollbackAction: "resource://normandy/actions/PreferenceRollbackAction.jsm",
  PreferenceRolloutAction: "resource://normandy/actions/PreferenceRolloutAction.jsm",
  ShowHeartbeatAction: "resource://normandy/actions/ShowHeartbeatAction.jsm",
  SinglePreferenceExperimentAction: "resource://normandy/actions/SinglePreferenceExperimentAction.jsm",
  Uptake: "resource://normandy/lib/Uptake.jsm",
});

var EXPORTED_SYMBOLS = ["ActionsManager"];

const log = LogManager.getLogger("recipe-runner");

/**
 * A class to manage the actions that recipes can use in Normandy.
 */
class ActionsManager {
  constructor() {
    this.finalized = false;

    const addonStudyAction = new AddonStudyAction();
    const singlePreferenceExperimentAction = new SinglePreferenceExperimentAction();

    this.localActions = {
      "addon-study": addonStudyAction,
      "console-log": new ConsoleLogAction(),
      "opt-out-study": addonStudyAction, // Legacy name used for addon-study on Normandy server
      "multi-preference-experiment": new PreferenceExperimentAction(),
      // Historically, this name meant SinglePreferenceExperimentAction.
      "preference-experiment": singlePreferenceExperimentAction,
      "preference-rollback": new PreferenceRollbackAction(),
      "preference-rollout": new PreferenceRolloutAction(),
      "single-preference-experiment": singlePreferenceExperimentAction,
      "show-heartbeat": new ShowHeartbeatAction(),
    };
  }

  async runRecipe(recipe) {
    let actionName = recipe.action;

    if (actionName in this.localActions) {
      log.info(`Executing recipe "${recipe.name}" (action=${recipe.action})`);
      const action = this.localActions[actionName];
      await action.runRecipe(recipe);
    } else {
      log.error(
        `Could not execute recipe ${recipe.name}:`,
        `Action ${recipe.action} is either missing or invalid.`
      );
      await Uptake.reportRecipe(recipe, Uptake.RECIPE_INVALID_ACTION);
    }
  }

  async finalize() {
    if (this.finalized) {
      throw new Error("ActionsManager has already been finalized");
    }
    this.finalized = true;

    // Finalize local actions
    for (const action of new Set(Object.values(this.localActions))) {
      action.finalize();
    }
  }
}
