/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { LogManager } = ChromeUtils.import(
  "resource://normandy/lib/LogManager.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonRollbackAction: "resource://normandy/actions/AddonRollbackAction.jsm",
  AddonRolloutAction: "resource://normandy/actions/AddonRolloutAction.jsm",
  AddonStudyAction: "resource://normandy/actions/AddonStudyAction.jsm",
  BranchedAddonStudyAction:
    "resource://normandy/actions/BranchedAddonStudyAction.jsm",
  ConsoleLogAction: "resource://normandy/actions/ConsoleLogAction.jsm",
  PreferenceExperimentAction:
    "resource://normandy/actions/PreferenceExperimentAction.jsm",
  PreferenceRollbackAction:
    "resource://normandy/actions/PreferenceRollbackAction.jsm",
  PreferenceRolloutAction:
    "resource://normandy/actions/PreferenceRolloutAction.jsm",
  ShowHeartbeatAction: "resource://normandy/actions/ShowHeartbeatAction.jsm",
  SinglePreferenceExperimentAction:
    "resource://normandy/actions/SinglePreferenceExperimentAction.jsm",
  Uptake: "resource://normandy/lib/Uptake.jsm",
});

var EXPORTED_SYMBOLS = ["ActionsManager"];

const log = LogManager.getLogger("recipe-runner");

const actionConstructors = {
  "addon-study": AddonStudyAction,
  "addon-rollback": AddonRollbackAction,
  "addon-rollout": AddonRolloutAction,
  "branched-addon-study": BranchedAddonStudyAction,
  "console-log": ConsoleLogAction,
  "multi-preference-experiment": PreferenceExperimentAction,
  "preference-rollback": PreferenceRollbackAction,
  "preference-rollout": PreferenceRolloutAction,
  "show-heartbeat": ShowHeartbeatAction,
  "single-preference-experiment": SinglePreferenceExperimentAction,
};

// Legacy names used by the server and older clients for actions.
const actionAliases = {
  "opt-out-study": "addon-study",
  "preference-experiment": "single-preference-experiment",
};

/**
 * A class to manage the actions that recipes can use in Normandy.
 */
class ActionsManager {
  constructor() {
    this.finalized = false;

    // Build a set of local actions, and aliases to them. The aliased names are
    // used by the server to keep compatibility with older clients.
    this.localActions = {};
    for (const [name, Constructor] of Object.entries(actionConstructors)) {
      this.localActions[name] = new Constructor();
    }
    for (const [alias, target] of Object.entries(actionAliases)) {
      this.localActions[alias] = this.localActions[target];
    }
  }

  static getCapabilities() {
    // Prefix each action name with "action." to turn it into a capability name.
    let capabilities = new Set();
    for (const actionName of Object.keys(actionConstructors)) {
      capabilities.add(`action.${actionName}`);
    }
    for (const actionAlias of Object.keys(actionAliases)) {
      capabilities.add(`action.${actionAlias}`);
    }
    return capabilities;
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
