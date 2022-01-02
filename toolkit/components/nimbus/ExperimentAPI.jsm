/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "ExperimentAPI",
  "NimbusFeatures",
  "_ExperimentFeature",
];

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
  setTimeout: "resource://gre/modules/Timer.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
  FeatureManifest: "resource://nimbus/FeatureManifest.js",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
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

function featuresCompat(branch) {
  if (!branch) {
    return [];
  }
  let { features } = branch;
  // In <=v1.5.0 of the Nimbus API, experiments had single feature
  if (!features) {
    features = [branch.feature];
  }

  return features;
}

const experimentBranchAccessor = {
  get: (target, prop) => {
    // Offer an API where we can access `branch.feature.*`.
    // This is a useful shorthand that hides the fact that
    // even single-feature recipes are still represented
    // as an array with 1 item
    if (!(prop in target) && target.features) {
      return target.features.find(f => f.featureId === prop);
    } else if (target.feature?.featureId === prop) {
      // Backwards compatibility for version 1.6.2 and older
      return target.feature;
    }

    return target[prop];
  },
};

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
  getExperiment({ slug, featureId } = {}) {
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
        branch: new Proxy(experimentData.branch, experimentBranchAccessor),
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
   * @param {{slug: string, featureId: string }}
   * @returns {Branch | null}
   */
  activateBranch({ slug, featureId }) {
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

    if (this._store._isReady) {
      let experiment = this.getExperiment(options);
      // Only if we have an experiment that matches what the caller requested
      if (experiment) {
        // If the store already has the experiment in the store then we should
        // notify. This covers the startup scenario or cases where listeners
        // are attached later than the `update` events.
        callback(fullEventName, experiment);
      }
    }

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
    return recipe?.branches.map(
      branch => new Proxy(branch, experimentBranchAccessor)
    );
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

/**
 * Singleton that holds lazy references to _ExperimentFeature instances
 * defined by the FeatureManifest
 */
const NimbusFeatures = {};
for (let feature in FeatureManifest) {
  XPCOMUtils.defineLazyGetter(NimbusFeatures, feature, () => {
    return new _ExperimentFeature(feature);
  });
}

class _ExperimentFeature {
  constructor(featureId, manifest) {
    this.featureId = featureId;
    this.prefGetters = {};
    this.manifest = manifest || FeatureManifest[featureId];
    if (!this.manifest) {
      Cu.reportError(
        `No manifest entry for ${featureId}. Please add one to toolkit/components/nimbus/FeatureManifest.js`
      );
    }
    this._didSendExposureEvent = false;
    this._onRemoteReady = null;
    this._waitForRemote = new Promise(
      resolve => (this._onRemoteReady = resolve)
    );
    this._listenForRemoteDefaults = this._listenForRemoteDefaults.bind(this);
    const variables = this.manifest?.variables || {};

    Object.keys(variables).forEach(key => {
      const { type, fallbackPref } = variables[key];
      if (fallbackPref) {
        XPCOMUtils.defineLazyPreferenceGetter(
          this.prefGetters,
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

    /**
     * There are multiple events that can resolve the wait for remote defaults:
     * 1. The feature can receive data via the RS update cycle
     * 2. The RS update cycle finished; no record exists for this feature
     * 3. User was enrolled in an experiment that targets this feature, resolve
     * because experiments take priority.
     */
    ExperimentAPI._store.on(
      "remote-defaults-finalized",
      this._listenForRemoteDefaults
    );
    this.onUpdate(this._listenForRemoteDefaults);
  }

  _listenForRemoteDefaults(eventName, reason) {
    if (
      // When the update cycle finished
      eventName === "remote-defaults-finalized" ||
      // remote default or experiment available
      reason === "experiment-updated" ||
      reason === "remote-defaults-update"
    ) {
      ExperimentAPI._store.off(
        "remote-defaults-updated",
        this._listenForRemoteDefaults
      );
      this.off(this._listenForRemoteDefaults);
      this._onRemoteReady();
    }
  }

  getPreferenceName(variable) {
    return this.manifest?.variables?.[variable]?.fallbackPref;
  }

  _getUserPrefsValues() {
    let userPrefs = {};
    Object.keys(this.manifest?.variables || {}).forEach(variable => {
      if (
        this.manifest.variables[variable].fallbackPref &&
        Services.prefs.prefHasUserValue(
          this.manifest.variables[variable].fallbackPref
        )
      ) {
        userPrefs[variable] = this.prefGetters[variable];
      }
    });

    return userPrefs;
  }

  /**
   * Wait for ExperimentStore to load giving access to experiment features that
   * do not have a pref cache and wait for remote defaults to load from Remote
   * Settings.
   *
   * @param {number} timeout Optional timeout parameter
   */
  async ready(timeout) {
    const REMOTE_DEFAULTS_TIMEOUT_MS = 15 * 1000; // 15 seconds
    await ExperimentAPI.ready();
    if (ExperimentAPI._store.hasRemoteDefaultsReady()) {
      this._onRemoteReady();
    } else {
      let remoteTimeoutId = setTimeout(
        this._onRemoteReady,
        timeout || REMOTE_DEFAULTS_TIMEOUT_MS
      );
      await this._waitForRemote;
      clearTimeout(remoteTimeoutId);
    }
  }

  /**
   * Lookup feature in active experiments and return enabled.
   * By default, this will send an exposure event.
   * @param {{defaultValue?: any}} options
   * @returns {obj} The feature value
   */
  isEnabled({ defaultValue = null } = {}) {
    const branch = ExperimentAPI.activateBranch({ featureId: this.featureId });

    let feature = featuresCompat(branch).find(
      ({ featureId }) => featureId === this.featureId
    );

    // First, try to return an experiment value if it exists.
    if (isBooleanValueDefined(feature?.enabled)) {
      return feature.enabled;
    }

    if (isBooleanValueDefined(this.getRemoteConfig()?.enabled)) {
      return this.getRemoteConfig().enabled;
    }

    let enabled;
    try {
      enabled = this.getVariable("enabled");
    } catch (e) {
      /* This is expected not all features have an enabled flag defined */
    }
    if (isBooleanValueDefined(enabled)) {
      return enabled;
    }

    return defaultValue;
  }

  /**
   * Lookup feature variables in experiments, prefs, and remote defaults.
   * @param {{defaultValues?: {[variableName: string]: any}}} options
   * @returns {{[variableName: string]: any}} The feature value
   */
  getAllVariables({ defaultValues = null } = {}) {
    // Any user pref will override any other configuration
    let userPrefs = this._getUserPrefsValues();
    const branch = ExperimentAPI.activateBranch({ featureId: this.featureId });
    const featureValue = featuresCompat(branch).find(
      ({ featureId }) => featureId === this.featureId
    )?.value;

    return {
      ...this.prefGetters,
      ...defaultValues,
      ...this.getRemoteConfig()?.variables,
      ...(featureValue || null),
      ...userPrefs,
    };
  }

  getVariable(variable) {
    const prefName = this.getPreferenceName(variable);
    const prefValue = prefName ? this.prefGetters[variable] : undefined;

    if (!this.manifest?.variables?.[variable]) {
      // Only throw in nightly/tests
      if (Cu.isInAutomation || AppConstants.NIGHTLY_BUILD) {
        throw new Error(
          `Nimbus: Warning - variable "${variable}" is not defined in FeatureManifest.js`
        );
      }
    }

    // If a user value is set for the defined preference, always return that first
    if (prefName && Services.prefs.prefHasUserValue(prefName)) {
      return prefValue;
    }

    // Next, check if an experiment is defined
    const branch = ExperimentAPI.activateBranch({
      featureId: this.featureId,
    });
    const experimentValue = featuresCompat(branch).find(
      ({ featureId }) => featureId === this.featureId
    )?.value?.[variable];

    if (typeof experimentValue !== "undefined") {
      return experimentValue;
    }

    // Next, check remote defaults
    const remoteValue = this.getRemoteConfig()?.variables?.[variable];
    if (typeof remoteValue !== "undefined") {
      return remoteValue;
    }
    // Return the default preference value
    return prefValue;
  }

  getRemoteConfig() {
    let remoteConfig = ExperimentAPI._store.getRemoteConfig(this.featureId);
    if (!remoteConfig) {
      return null;
    }

    return remoteConfig;
  }

  recordExposureEvent({ once = false } = {}) {
    if (once && this._didSendExposureEvent) {
      return;
    }

    let experimentData = ExperimentAPI.getExperiment({
      featureId: this.featureId,
    });

    // Exposure only sent if user is enrolled in an experiment
    if (experimentData) {
      ExperimentAPI.recordExposureEvent({
        featureId: this.featureId,
        experimentSlug: experimentData.slug,
        branchSlug: experimentData.branch?.slug,
      });
      this._didSendExposureEvent = true;
    }
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
      variables: this.getAllVariables(),
      experiment: ExperimentAPI.getExperimentMetaData({
        featureId: this.featureId,
      }),
      fallbackPrefs:
        this.prefGetters &&
        Object.keys(this.prefGetters).map(prefName => [
          prefName,
          this.prefGetters[prefName],
        ]),
      userPrefs: this._getUserPrefsValues(),
      remoteDefaults: this.getRemoteConfig(),
    };
  }
}

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_store", function() {
  return IS_MAIN_PROCESS ? ExperimentManager.store : new ExperimentStore();
});

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_remoteSettingsClient", function() {
  return RemoteSettings(COLLECTION_ID);
});
