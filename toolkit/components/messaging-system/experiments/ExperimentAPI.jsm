/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * @typedef {import("./@types/ExperimentManager").Enrollment} Enrollment
 * @typedef {import("./@types/ExperimentManager").FeatureConfig} FeatureConfig
 */

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

const COLLECTION_ID_PREF = "messaging-system.rsexperimentloader.collection_id";
const COLLECTION_ID_FALLBACK = "nimbus-desktop-experiments";
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "COLLECTION_ID",
  COLLECTION_ID_PREF,
  COLLECTION_ID_FALLBACK
);

const ExperimentAPI = {
  /**
   * @returns {Promise} Resolves when the API has synchronized to the main store
   */
  ready() {
    return this._store.ready();
  },

  /**
   * Returns an experiment, including all its metadata
   * Sends exposure ping
   *
   * @param {{slug?: string, featureId?: string}} options slug = An experiment identifier
   * or feature = a stable identifier for a type of experiment
   * @returns {{slug: string, active: bool, exposurePingSent: bool}} A matching experiment if one is found.
   */
  getExperiment({ slug, featureId, sendExposurePing } = {}) {
    if (!slug && !featureId) {
      throw new Error(
        "getExperiment(options) must include a slug or a feature."
      );
    }
    let experimentData;
    if (slug) {
      experimentData = this._store.get(slug);
    } else if (featureId) {
      experimentData = this._store.getExperimentForFeature(featureId);
    }
    if (experimentData) {
      return {
        slug: experimentData.slug,
        active: experimentData.active,
        exposurePingSent: experimentData.exposurePingSent,
        branch: this.getFeatureBranch({ featureId, sendExposurePing }),
      };
    }

    return null;
  },

  /**
   * Return experiment slug its status and the enrolled branch slug
   * Does NOT send exposure ping because you only have access to the slugs
   */
  getExperimentMetaData({ slug, featureId }) {
    if (!slug && !featureId) {
      throw new Error(
        "getExperiment(options) must include a slug or a feature."
      );
    }

    let experimentData;
    if (slug) {
      experimentData = this._store.get(slug);
    } else if (featureId) {
      experimentData = this._store.getExperimentForFeature(featureId);
    }
    if (experimentData) {
      return {
        slug: experimentData.slug,
        active: experimentData.active,
        exposurePingSent: experimentData.exposurePingSent,
        branch: { slug: experimentData.branch.slug },
      };
    }

    return null;
  },

  /**
   * Lookup feature in active experiments and return status.
   * Sends exposure ping
   * @param {string} featureId Feature to lookup
   * @param {boolean} defaultValue
   * @returns {boolean}
   */
  isFeatureEnabled(featureId, defaultValue) {
    const branch = this.getFeatureBranch({ featureId });
    if (branch?.feature.enabled !== undefined) {
      return branch.feature.enabled;
    }
    return defaultValue;
  },

  /**
   * Lookup feature in active experiments and return value.
   * By default, this will send an exposure event.
   * @param {{featureId: string, sendExposurePing: boolean}} options
   * @returns {obj} The feature value
   */
  getFeatureValue(options) {
    return this._store.activateBranch(options)?.feature.value;
  },

  /**
   * Lookup feature in active experiments and returns the entire branch.
   * By default, this will send an exposure event.
   * @param {{featureId: string, sendExposurePing: boolean}} options
   * @returns {Branch}
   */
  getFeatureBranch(options) {
    return this._store.activateBranch(options);
  },

  /**
   * Registers an event listener.
   * The following event names are used:
   * `update` - an experiment is updated, for example it is no longer active
   *
   * @param {string} eventName must follow the pattern `event:slug-name`
   * @param {{slug?: string, featureId: string?}} options
   * @param {function} callback

   * @returns {void}
   */
  on(eventName, options, callback) {
    if (!options) {
      throw new Error("Please include an experiment slug or featureId");
    }
    let fullEventName = `${eventName}:${options.slug || options.featureId}`;

    // The update event will always fire after the event listener is added, either
    // immediately if it is already ready, or on ready
    this._store.ready().then(() => {
      let experiment = this.getExperiment(options);
      // Only if we have an experiment that matches what the caller requested
      if (experiment) {
        // If the store already has the experiment in the store then we should
        // notify. This covers the startup scenario or cases where listeners
        // are attached later than the `update` events.
        callback(fullEventName, experiment);
      }
    });

    this._store.on(fullEventName, callback);
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
        filters: { slug },
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
    return recipe?.branches;
  },

  recordExposureEvent(name, { sent, experimentSlug, branchSlug }) {
    if (!IS_MAIN_PROCESS) {
      Cu.reportError("Need to call from Parent process");
      return false;
    }
    if (sent) {
      return false;
    }

    // Notify listener to record that the ping was sent
    this._store._emitExperimentExposure({
      featureId: name,
      experimentSlug,
      branchSlug,
    });

    return true;
  },
};

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_store", function() {
  return IS_MAIN_PROCESS ? ExperimentManager.store : new ExperimentStore();
});

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_remoteSettingsClient", function() {
  return RemoteSettings(COLLECTION_ID);
});
