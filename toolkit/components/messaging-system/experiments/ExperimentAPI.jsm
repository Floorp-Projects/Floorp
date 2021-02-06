/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ExperimentAPI", "ExperimentFeature"];

/**
 * FEATURE MANIFEST
 * =================
 * Features must be added here to be accessible through the ExperimentFeature() API.
 * In the future, this will be moved to a configuration file.
 */
const MANIFEST = {
  aboutwelcome: {
    description: "The about:welcome page",
    enabledFallbackPref: "browser.aboutwelcome.enabled",
    variables: {
      value: {
        type: "json",
        fallbackPref: "browser.aboutwelcome.overrideContent",
      },
    },
  },
  newtab: {
    description: "The about:newtab page",
    variables: {
      value: {
        type: "json",
        fallbackPref: "browser.newtab.experiments.value",
      },
    },
  },
  "password-autocomplete": {
    description: "A special autocomplete UI for password fields.",
  },
};

function isBooleanValueDefined(value) {
  return typeof value === "boolean";
}

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

function parseJSON(value) {
  if (value) {
    try {
      return JSON.parse(value);
    } catch (e) {
      Cu.reportError(e);
    }
  }
  return null;
}

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
    try {
      if (slug) {
        experimentData = this._store.get(slug);
      } else if (featureId) {
        experimentData = this._store.getExperimentForFeature(featureId);
      }
    } catch (e) {
      Cu.reportError(e);
    }
    if (experimentData) {
      return {
        slug: experimentData.slug,
        active: experimentData.active,
        exposurePingSent: experimentData.exposurePingSent,
        branch: this.activateBranch({ featureId, sendExposurePing }),
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
    try {
      if (slug) {
        experimentData = this._store.get(slug);
      } else if (featureId) {
        experimentData = this._store.getExperimentForFeature(featureId);
      }
    } catch (e) {
      Cu.reportError(e);
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
   * Return FeatureConfig from first active experiment where it can be found
   * @param {{slug: string, featureId: string, sendExposurePing: bool}}
   * @returns {Branch | null}
   */
  activateBranch({ slug, featureId, sendExposurePing = true }) {
    let experiment = null;
    try {
      if (slug) {
        experiment = this._store.get(slug);
      } else if (featureId) {
        experiment = this._store.getExperimentForFeature(featureId);
      }
    } catch (e) {
      Cu.reportError(e);
    }

    if (!experiment) {
      return null;
    }

    if (sendExposurePing) {
      this._store._emitExperimentExposure({
        experimentSlug: experiment.slug,
        branchSlug: experiment?.branch?.slug,
        featureId,
      });
    }

    // Default to null for feature-less experiments where we're only
    // interested in exposure.
    return experiment?.branch || null;
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

class ExperimentFeature {
  static MANIFEST = MANIFEST;
  constructor(featureId, manifest) {
    this.featureId = featureId;
    this.defaultPrefValues = {};
    this.manifest = manifest || ExperimentFeature.MANIFEST[featureId];
    if (!this.manifest) {
      Cu.reportError(
        `No manifest entry for ${featureId}. Please add one to toolkit/components/messaging-system/experiments/ExperimentAPI.jsm`
      );
    }
    const variables = this.manifest?.variables || {};

    // Add special default variable.
    if (!variables.enabled) {
      variables.enabled = {
        type: "boolean",
        fallbackPref: this.manifest?.enabledFallbackPref,
      };
    }

    Object.keys(variables).forEach(key => {
      const { type, fallbackPref } = variables[key];
      if (fallbackPref) {
        XPCOMUtils.defineLazyPreferenceGetter(
          this.defaultPrefValues,
          key,
          fallbackPref,
          null,
          () => {
            ExperimentAPI._store._emitFeatureUpdate(
              this.featureId,
              "pref-updated"
            );
          },
          type === "json" ? parseJSON : val => val
        );
      }
    });
  }

  /**
   * Lookup feature in active experiments and return enabled.
   * By default, this will send an exposure event.
   * @param {{sendExposurePing: boolean, defaultValue?: any}} options
   * @returns {obj} The feature value
   */
  isEnabled({ sendExposurePing, defaultValue = null } = {}) {
    const branch = ExperimentAPI.activateBranch({
      featureId: this.featureId,
      sendExposurePing,
    });

    // First, try to return an experiment value if it exists.
    if (isBooleanValueDefined(branch?.feature.enabled)) {
      return branch.feature.enabled;
    }

    // Then check the fallback pref, if it is defined
    if (isBooleanValueDefined(this.defaultPrefValues.enabled)) {
      return this.defaultPrefValues.enabled;
    }

    // Finally, return options.defaulValue if neither was found
    return defaultValue;
  }

  /**
   * Lookup feature in active experiments and return value.
   * By default, this will send an exposure event.
   * @param {{sendExposurePing: boolean, defaultValue?: any}} options
   * @returns {obj} The feature value
   */
  getValue({ sendExposurePing, defaultValue = null } = {}) {
    const branch = ExperimentAPI.activateBranch({
      featureId: this.featureId,
      sendExposurePing,
    });
    if (branch?.feature?.value) {
      return branch.feature.value;
    }

    return this.defaultPrefValues.value || defaultValue;
  }

  onUpdate(callback) {
    ExperimentAPI._store._onFeatureUpdate(this.featureId, callback);
  }

  off(callback) {
    ExperimentAPI._store._offFeatureUpdate(this.featureId, callback);
  }

  debug() {
    return {
      enabled: this.isEnabled(),
      value: this.getValue(),
      experiment: ExperimentAPI.getExperimentMetaData({
        featureId: this.featureId,
      }),
      fallbackPrefs:
        this.defaultPrefValues &&
        Object.keys(this.defaultPrefValues).map(prefName => [
          prefName,
          this.defaultPrefValues[prefName],
        ]),
    };
  }
}

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_store", function() {
  return IS_MAIN_PROCESS ? ExperimentManager.store : new ExperimentStore();
});

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_remoteSettingsClient", function() {
  return RemoteSettings(COLLECTION_ID);
});
