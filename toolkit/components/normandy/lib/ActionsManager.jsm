/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { LogManager } = ChromeUtils.import(
  "resource://normandy/lib/LogManager.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonRollbackAction: "resource://normandy/actions/AddonRollbackAction.jsm",
  AddonRolloutAction: "resource://normandy/actions/AddonRolloutAction.jsm",
  BaseAction: "resource://normandy/actions/BaseAction.jsm",
  BranchedAddonStudyAction:
    "resource://normandy/actions/BranchedAddonStudyAction.jsm",
  ConsoleLogAction: "resource://normandy/actions/ConsoleLogAction.jsm",
  MessagingExperimentAction:
    "resource://normandy/actions/MessagingExperimentAction.jsm",
  PreferenceExperimentAction:
    "resource://normandy/actions/PreferenceExperimentAction.jsm",
  PreferenceRollbackAction:
    "resource://normandy/actions/PreferenceRollbackAction.jsm",
  PreferenceRolloutAction:
    "resource://normandy/actions/PreferenceRolloutAction.jsm",
  ShowHeartbeatAction: "resource://normandy/actions/ShowHeartbeatAction.jsm",
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

    this.localActions = {};
    for (const [name, Constructor] of Object.entries(
      ActionsManager.actionConstructors
    )) {
      this.localActions[name] = new Constructor();
    }
  }

  static actionConstructors = {
    "addon-rollback": lazy.AddonRollbackAction,
    "addon-rollout": lazy.AddonRolloutAction,
    "branched-addon-study": lazy.BranchedAddonStudyAction,
    "console-log": lazy.ConsoleLogAction,
    "messaging-experiment": lazy.MessagingExperimentAction,
    "multi-preference-experiment": lazy.PreferenceExperimentAction,
    "preference-rollback": lazy.PreferenceRollbackAction,
    "preference-rollout": lazy.PreferenceRolloutAction,
    "show-heartbeat": lazy.ShowHeartbeatAction,
  };

  static getCapabilities() {
    // Prefix each action name with "action." to turn it into a capability name.
    let capabilities = new Set();
    for (const actionName of Object.keys(ActionsManager.actionConstructors)) {
      capabilities.add(`action.${actionName}`);
    }
    return capabilities;
  }

  async processRecipe(recipe, suitability) {
    let actionName = recipe.action;

    if (actionName in this.localActions) {
      log.info(`Executing recipe "${recipe.name}" (action=${recipe.action})`);
      const action = this.localActions[actionName];
      await action.processRecipe(recipe, suitability);

      // If the recipe doesn't have matching capabilities, then a missing action
      // is expected. In this case, don't send an error
    } else if (
      suitability !== lazy.BaseAction.suitability.CAPABILITIES_MISMATCH
    ) {
      log.error(
        `Could not execute recipe ${recipe.name}:`,
        `Action ${recipe.action} is either missing or invalid.`
      );
      await lazy.Uptake.reportRecipe(recipe, lazy.Uptake.RECIPE_INVALID_ACTION);
    }
  }

  async finalize(options) {
    if (this.finalized) {
      throw new Error("ActionsManager has already been finalized");
    }
    this.finalized = true;

    // Finalize local actions
    for (const action of Object.values(this.localActions)) {
      action.finalize(options);
    }
  }
}
