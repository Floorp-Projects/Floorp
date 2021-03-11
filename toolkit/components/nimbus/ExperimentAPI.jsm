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
      screens: {
        type: "json",
        fallbackPref: "browser.aboutwelcome.screens",
      },
      design: {
        type: "string",
        fallbackPref: "browser.aboutwelcome.design",
      },
      skipFocus: {
        type: "boolean",
        fallbackPref: "browser.aboutwelcome.skipFocus",
      },
    },
  },
  newtab: {
    description: "The about:newtab page",
    variables: {
      newNewtabExperienceEnabled: {
        type: "boolean",
        fallbackPref:
          "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
      },
      customizationMenuEnabled: {
        type: "boolean",
        fallbackPref:
          "browser.newtabpage.activity-stream.customizationMenu.enabled",
      },
      prefsButtonIcon: {
        type: "string",
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
  ExperimentStore: "resource://nimbus/lib/ExperimentStore.jsm",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
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
const EXPOSURE_EVENT_CATEGORY = "normandy";
const EXPOSURE_EVENT_METHOD = "expose";
const EXPOSURE_EVENT_OBJECT = "nimbus_experiment";

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
   * Sends exposure event
   *
   * @param {{slug?: string, featureId?: string}} options slug = An experiment identifier
   * or feature = a stable identifier for a type of experiment
   * @returns {{slug: string, active: bool}} A matching experiment if one is found.
   */
  getExperiment({ slug, featureId, sendExposureEvent } = {}) {
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
        branch: this.activateBranch({ slug, featureId, sendExposureEvent }),
      };
    }

    return null;
  },

  /**
   * Return experiment slug its status and the enrolled branch slug
   * Does NOT send exposure event because you only have access to the slugs
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
        branch: { slug: experimentData.branch.slug },
      };
    }

    return null;
  },

  /**
   * Return FeatureConfig from first active experiment where it can be found
   * @param {{slug: string, featureId: string, sendExposureEvent: bool}}
   * @returns {Branch | null}
   */
  activateBranch({ slug, featureId, sendExposureEvent }) {
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

    if (sendExposureEvent) {
      this.recordExposureEvent({
        experimentSlug: experiment.slug,
        branchSlug: experiment.branch.slug,
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

  recordExposureEvent({ featureId, experimentSlug, branchSlug }) {
    Services.telemetry.setEventRecordingEnabled(EXPOSURE_EVENT_CATEGORY, true);
    try {
      Services.telemetry.recordEvent(
        EXPOSURE_EVENT_CATEGORY,
        EXPOSURE_EVENT_METHOD,
        EXPOSURE_EVENT_OBJECT,
        experimentSlug,
        {
          branchSlug,
          featureId,
        }
      );
    } catch (e) {
      Cu.reportError(e);
    }
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

    // Add special enabled flag
    if (this.manifest?.enabledFallbackPref) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "enabled",
        this.manifest?.enabledFallbackPref,
        null,
        () => {
          ExperimentAPI._store._emitFeatureUpdate(
            this.featureId,
            "pref-updated"
          );
        }
      );
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

  ready() {
    return ExperimentAPI.ready();
  }

  /**
   * Lookup feature in active experiments and return enabled.
   * By default, this will send an exposure event.
   * @param {{sendExposureEvent: boolean, defaultValue?: any}} options
   * @returns {obj} The feature value
   */
  isEnabled({ sendExposureEvent, defaultValue = null } = {}) {
    const branch = ExperimentAPI.activateBranch({
      featureId: this.featureId,
      sendExposureEvent,
    });

    // First, try to return an experiment value if it exists.
    if (isBooleanValueDefined(branch?.feature.enabled)) {
      return branch.feature.enabled;
    }

    // Then check the fallback pref, if it is defined
    if (isBooleanValueDefined(this.enabled)) {
      return this.enabled;
    }

    // Finally, return options.defaulValue if neither was found
    return defaultValue;
  }

  /**
   * Lookup feature in active experiments and return value.
   * By default, this will send an exposure event.
   * @param {{sendExposureEvent: boolean, defaultValue?: any}} options
   * @returns {obj} The feature value
   */
  getValue({ sendExposureEvent, defaultValue = null } = {}) {
    const branch = ExperimentAPI.activateBranch({
      featureId: this.featureId,
      sendExposureEvent,
    });
    if (branch?.feature?.value) {
      return branch.feature.value;
    }

    return Object.keys(this.defaultPrefValues).length
      ? this.defaultPrefValues
      : defaultValue;
  }

  recordExposureEvent() {
    ExperimentAPI.activateBranch({
      featureId: this.featureId,
      sendExposureEvent: true,
    });
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
