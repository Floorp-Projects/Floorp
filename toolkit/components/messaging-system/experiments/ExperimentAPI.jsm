/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ExperimentAPI"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ExperimentStore:
    "resource://messaging-system/experiments/ExperimentStore.jsm",
  ExperimentManager:
    "resource://messaging-system/experiments/ExperimentManager.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
});

const IS_MAIN_PROCESS =
  Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

const COLLECTION_ID = "messaging-experiments";

const ExperimentAPI = {
  /**
   * @returns {Promise} Resolves when the API has synchronized to the main store
   */
  ready() {
    return this._store.ready();
  },

  /**
   * Returns an experiment, including all its metadata
   *
   * @param {{slug?: string, group?: string}} options slug = An experiment identifier
   * or group = a stable identifier for a group of experiments
   * @returns {Enrollment|undefined} A matching experiment if one is found.
   */
  getExperiment({ slug, group } = {}) {
    if (slug) {
      return this._store.get(slug);
    } else if (group) {
      return this._store.getExperimentForGroup(group);
    }
    throw new Error("getExperiment(options) must include a slug or a group.");
  },

  /**
   * Returns the value of the selected branch for a given experiment
   *
   * @param {{slug?: string, group?: string}} options slug = An experiment identifier
   * or group = a stable identifier for a group of experiments
   * @returns {any} The selected value of the active branch of the experiment
   */
  getValue(options) {
    return this.getExperiment(options)?.branch.value;
  },

  /**
   * Registers an event listener.
   * The following event names are used:
   * `update` - an experiment is updated, for example it is no longer active
   *
   * @param {string} eventName must follow the pattern `event:slug-name`
   * @param {function} callback
   * @returns {void}
   */
  on(eventName, callback) {
    this._store.on(eventName, callback);
  },

  /**
   * Deregisters an event listener.
   * @param {string} eventName
   * @param {function} callback
   */
  off(eventName, callback) {
    this._store.off(eventName, callback);
  },

  /**
   * Returns the recipe for a given experiment slug
   *
   * This should noly be called from the main process.
   *
   * Note that the recipe is directly fetched from RemoteSettings, which has
   * all the recipe metadata available without relying on the `this._store`.
   * Therefore, calling this function does not require to call `this.ready()` first.
   *
   * @param slug {String} An experiment identifier
   * @returns {Recipe|undefined} A matching experiment recipe if one is found
   */
  async getRecipe(slug) {
    if (!IS_MAIN_PROCESS) {
      throw new Error(
        "getRecipe() should only be called from the main process"
      );
    }

    let recipe;

    try {
      [recipe] = await this._remoteSettingsClient.get({
        // Do not sync the RS store, let RemoteSettingsExperimentLoader do that
        syncIfEmpty: false,
        filters: {
          "arguments.slug": slug,
        },
      });
    } catch (e) {
      Cu.reportError(e);
      recipe = undefined;
    }

    return recipe;
  },

  /**
   * Returns all the branches for a given experiment slug
   *
   * This should only be called from the main process. Like `getRecipe()`,
   * calling this function does not require to call `this.ready()` first.
   *
   * @param slug {String} An experiment identifier
   * @returns {[Branches]|undefined} An array of branches for the given slug
   */
  async getAllBranches(slug) {
    if (!IS_MAIN_PROCESS) {
      throw new Error(
        "getAllBranches() should only be called from the main process"
      );
    }

    const recipe = await this.getRecipe(slug);
    return recipe?.arguments.branches;
  },
};

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_store", function() {
  return IS_MAIN_PROCESS ? ExperimentManager.store : new ExperimentStore();
});

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_remoteSettingsClient", function() {
  return RemoteSettings(COLLECTION_ID);
});
