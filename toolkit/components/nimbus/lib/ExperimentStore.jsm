/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ExperimentStore"];

const { SharedDataMap } = ChromeUtils.import(
  "resource://nimbus/lib/SharedDataMap.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  FeatureManifest: "resource://nimbus/FeatureManifest.js",
  PrefUtils: "resource://normandy/lib/PrefUtils.jsm",
});

const IS_MAIN_PROCESS =
  Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;
const REMOTE_DEFAULTS_KEY = "__REMOTE_DEFAULTS";

// This branch is used to store experiment data
const SYNC_DATA_PREF_BRANCH = "nimbus.syncdatastore.";
// This branch is used to store remote rollouts
const SYNC_DEFAULTS_PREF_BRANCH = "nimbus.syncdefaultsstore.";
let tryJSONParse = data => {
  try {
    return JSON.parse(data);
  } catch (e) {}

  return null;
};
XPCOMUtils.defineLazyGetter(this, "syncDataStore", () => {
  // Pref name changed in bug 1691516 and we want to clear the old pref for users
  const previousPrefName = "messaging-system.syncdatastore.data";
  try {
    if (IS_MAIN_PROCESS) {
      Services.prefs.clearUserPref(previousPrefName);
    }
  } catch (e) {}

  let experimentsPrefBranch = Services.prefs.getBranch(SYNC_DATA_PREF_BRANCH);
  let defaultsPrefBranch = Services.prefs.getBranch(SYNC_DEFAULTS_PREF_BRANCH);
  return {
    _tryParsePrefValue(branch, pref) {
      try {
        return tryJSONParse(branch.getStringPref(pref, ""));
      } catch (e) {
        /* This is expected if we don't have anything stored */
      }

      return null;
    },
    _trySetPrefValue(branch, pref, value) {
      try {
        branch.setStringPref(pref, JSON.stringify(value));
      } catch (e) {
        Cu.reportError(e);
      }
    },
    _trySetTypedPrefValue(pref, value) {
      let variableType = typeof value;
      switch (variableType) {
        case "boolean":
          Services.prefs.setBoolPref(pref, value);
          break;
        case "number":
          Services.prefs.setIntPref(pref, value);
          break;
        case "string":
          Services.prefs.setStringPref(pref, value);
          break;
        case "object":
          Services.prefs.setStringPref(pref, JSON.stringify(value));
          break;
      }
    },
    _clearBranchChildValues(prefBranch) {
      const variablesBranch = Services.prefs.getBranch(prefBranch);
      const prefChildList = variablesBranch.getChildList("");
      for (let variable of prefChildList) {
        variablesBranch.clearUserPref(variable);
      }
    },
    /**
     * Given a branch pref returns all child prefs and values
     * { childPref: value }
     * where value is parsed to the appropriate type
     *
     * @returns {Object[]}
     */
    _getBranchChildValues(prefBranch, featureId) {
      const branch = Services.prefs.getBranch(prefBranch);
      const prefChildList = branch.getChildList("");
      let values = {};
      if (!prefChildList.length) {
        return null;
      }
      for (const childPref of prefChildList) {
        let prefName = `${prefBranch}${childPref}`;
        let value = PrefUtils.getPref(prefName);
        // Try to parse string values that could be stringified objects
        if (FeatureManifest[featureId]?.variables[childPref]?.type === "json") {
          let parsedValue = tryJSONParse(value);
          if (parsedValue) {
            value = parsedValue;
          }
        }
        values[childPref] = value;
      }

      return values;
    },
    get(featureId) {
      let metadata = this._tryParsePrefValue(experimentsPrefBranch, featureId);
      if (!metadata) {
        return null;
      }
      let prefBranch = `${SYNC_DATA_PREF_BRANCH}${featureId}.`;
      metadata.branch.feature.value = this._getBranchChildValues(
        prefBranch,
        featureId
      );

      return metadata;
    },
    getDefault(featureId) {
      let metadata = this._tryParsePrefValue(defaultsPrefBranch, featureId);
      if (!metadata) {
        return null;
      }
      let prefBranch = `${SYNC_DEFAULTS_PREF_BRANCH}${featureId}.`;
      metadata.variables = this._getBranchChildValues(prefBranch, featureId);

      return metadata;
    },
    set(featureId, value) {
      /* If the enrollment branch has variables we store those separately
       * in pref branches of appropriate type:
       * { featureId: "foo", value: { enabled: true } }
       * gets stored as `${SYNC_DATA_PREF_BRANCH}foo.enabled=true`
       */
      if (value.branch?.feature?.value) {
        for (let variable of Object.keys(value.branch.feature.value)) {
          let prefName = `${SYNC_DATA_PREF_BRANCH}${featureId}.${variable}`;
          this._trySetTypedPrefValue(
            prefName,
            value.branch.feature.value[variable]
          );
        }
        this._trySetPrefValue(experimentsPrefBranch, featureId, {
          ...value,
          branch: {
            ...value.branch,
            feature: {
              ...value.branch.feature,
              value: null,
            },
          },
        });
      } else {
        this._trySetPrefValue(experimentsPrefBranch, featureId, value);
      }
    },
    setDefault(featureId, value) {
      /* We store configuration variables separately in pref branches of
       * appropriate type:
       * (feature: "foo") { variables: { enabled: true } }
       * gets stored as `${SYNC_DEFAULTS_PREF_BRANCH}foo.enabled=true`
       */
      for (let variable of Object.keys(value.variables || {})) {
        let prefName = `${SYNC_DEFAULTS_PREF_BRANCH}${featureId}.${variable}`;
        this._trySetTypedPrefValue(prefName, value.variables[variable]);
      }
      this._trySetPrefValue(defaultsPrefBranch, featureId, {
        ...value,
        variables: null,
      });
    },
    getAllDefaultBranches() {
      return defaultsPrefBranch.getChildList("").filter(
        // Filter out remote defaults variable prefs
        pref => !pref.includes(".")
      );
    },
    delete(featureId) {
      const prefBranch = `${SYNC_DATA_PREF_BRANCH}${featureId}.`;
      this._clearBranchChildValues(prefBranch);
      try {
        experimentsPrefBranch.clearUserPref(featureId);
      } catch (e) {}
    },
    deleteDefault(featureId) {
      let prefBranch = `${SYNC_DEFAULTS_PREF_BRANCH}${featureId}.`;
      this._clearBranchChildValues(prefBranch);
      try {
        defaultsPrefBranch.clearUserPref(featureId);
      } catch (e) {}
    },
  };
});

const DEFAULT_STORE_ID = "ExperimentStoreData";

class ExperimentStore extends SharedDataMap {
  static SYNC_DATA_PREF_BRANCH = SYNC_DATA_PREF_BRANCH;
  static SYNC_DEFAULTS_PREF_BRANCH = SYNC_DEFAULTS_PREF_BRANCH;

  constructor(sharedDataKey, options = { isParent: IS_MAIN_PROCESS }) {
    super(sharedDataKey || DEFAULT_STORE_ID, options);
  }

  async init() {
    await super.init();

    this.getAllActive().forEach(({ branch }) => {
      if (branch?.feature?.featureId) {
        this._emitFeatureUpdate(
          branch.feature.featureId,
          "feature-experiment-loaded"
        );
      }
    });

    Services.tm.idleDispatchToMainThread(() => this._cleanupOldRecipes());
  }

  /**
   * Given a feature identifier, find an active experiment that matches that feature identifier.
   * This assumes, for now, that there is only one active experiment per feature per browser.
   * Does not activate the experiment (send an exposure event)
   *
   * @param {string} featureId
   * @returns {Enrollment|undefined} An active experiment if it exists
   * @memberof ExperimentStore
   */
  getExperimentForFeature(featureId) {
    return (
      this.getAllActive().find(
        experiment =>
          experiment.featureIds?.includes(featureId) ||
          // Supports <v1.3.0, which was when .featureIds was added
          experiment.branch?.feature?.featureId === featureId
        // Default to the pref store if data is not yet ready
      ) || syncDataStore.get(featureId)
    );
  }

  /**
   * Check if an active experiment already exists for a feature.
   * Does not activate the experiment (send an exposure event)
   *
   * @param {string} featureId
   * @returns {boolean} Does an active experiment exist for that feature?
   * @memberof ExperimentStore
   */
  hasExperimentForFeature(featureId) {
    if (!featureId) {
      return false;
    }
    return !!this.getExperimentForFeature(featureId);
  }

  /**
   * @returns {Enrollment[]}
   */
  getAll() {
    let data = [];
    try {
      data = Object.values(this._data || {});
    } catch (e) {
      Cu.reportError(e);
    }

    return data;
  }

  /**
   * @returns {Enrollment[]}
   */
  getAllActive() {
    return this.getAll().filter(experiment => experiment.active);
  }

  /**
   * Remove inactive enrollments older than 6 months
   */
  _cleanupOldRecipes() {
    // Roughly six months
    const threshold = 15552000000;
    const nowTimestamp = new Date().getTime();
    const recipesToRemove = this.getAll().filter(
      experiment =>
        !experiment.active &&
        // Flip the comparison here to catch scenarios in which lastSeen is
        // invalid or undefined. The result with be a comparison with NaN
        // which is always false
        !(nowTimestamp - new Date(experiment.lastSeen).getTime() < threshold)
    );
    this._removeEntriesByKeys(recipesToRemove.map(r => r.slug));
  }

  _emitExperimentUpdates(experiment) {
    this.emit(`update:${experiment.slug}`, experiment);
    if (experiment.branch.feature) {
      this.emit(`update:${experiment.branch.feature.featureId}`, experiment);
      this._emitFeatureUpdate(
        experiment.branch.feature.featureId,
        "experiment-updated"
      );
    }
  }

  _emitFeatureUpdate(featureId, reason) {
    this.emit(`featureUpdate:${featureId}`, reason);
  }

  _onFeatureUpdate(featureId, callback) {
    this.on(`featureUpdate:${featureId}`, callback);
  }

  _offFeatureUpdate(featureId, callback) {
    this.off(`featureUpdate:${featureId}`, callback);
  }

  /**
   * @param {Enrollment} experiment
   */
  _updateSyncStore(experiment) {
    let featureId = experiment.branch.feature?.featureId;
    if (
      FeatureManifest[featureId]?.isEarlyStartup ||
      experiment.branch.feature?.isEarlyStartup
    ) {
      if (!experiment.active) {
        // Remove experiments on un-enroll, no need to check if it exists
        syncDataStore.delete(featureId);
      } else {
        syncDataStore.set(featureId, experiment);
      }
    }
  }

  /**
   * Add an experiment. Short form for .set(slug, experiment)
   * @param {Enrollment} experiment
   */
  addExperiment(experiment) {
    if (!experiment || !experiment.slug) {
      throw new Error(
        `Tried to add an experiment but it didn't have a .slug property.`
      );
    }
    this.set(experiment.slug, experiment);
    this._updateSyncStore(experiment);
    this._emitExperimentUpdates(experiment);
  }

  /**
   * Merge new properties into the properties of an existing experiment
   * @param {string} slug
   * @param {Partial<Enrollment>} newProperties
   */
  updateExperiment(slug, newProperties) {
    const oldProperties = this.get(slug);
    if (!oldProperties) {
      throw new Error(
        `Tried to update experiment ${slug} bug it doesn't exist`
      );
    }
    const updatedExperiment = { ...oldProperties, ...newProperties };
    this.set(slug, updatedExperiment);
    this._updateSyncStore(updatedExperiment);
    this._emitExperimentUpdates(updatedExperiment);
  }

  /**
   * Remove any unused remote configurations and send the end event
   *
   * @param {Array} activeFeatureIds The set of all feature ids with matching configs during an update
   * @memberof ExperimentStore
   */
  finalizeRemoteConfigs(activeFeatureConfigIds) {
    if (!activeFeatureConfigIds) {
      throw new Error("You must pass in an array of active feature ids.");
    }
    // If we haven't seen this feature in any of the configurations
    // processed then we should clean up the matching pref cache and in-memory store
    for (let featureId of this.getAllExistingRemoteConfigIds()) {
      if (!activeFeatureConfigIds.includes(featureId)) {
        this.deleteRemoteConfig(featureId);
      }
    }

    // In case no features exist we want to at least initialize with an empty
    // object to signal that we completed the initial fetch step
    if (!activeFeatureConfigIds.length) {
      // Wait for ready, in the case users have opted out we finalize early
      // and things might not be ready yet
      this.ready().then(() => this.setNonPersistent(REMOTE_DEFAULTS_KEY, {}));
    }

    // Notify all ExperimentFeature instances that the Remote Defaults cycle finished
    // this will resolve the `onRemoteReady` promise for features that do not
    // have any remote data available.
    this.emit("remote-defaults-finalized");
  }

  /**
   * Store the remote configuration once loaded from Remote Settings.
   * @param {string} featureId The feature we want to update with remote defaults
   * @param {object} configuration The remote value
   */
  updateRemoteConfigs(featureId, configuration) {
    const remoteConfigState = this.get(REMOTE_DEFAULTS_KEY);
    this.setNonPersistent(REMOTE_DEFAULTS_KEY, {
      ...remoteConfigState,
      [featureId]: { ...configuration },
    });
    if (
      FeatureManifest[featureId]?.isEarlyStartup ||
      configuration.isEarlyStartup
    ) {
      syncDataStore.setDefault(featureId, configuration);
    }
    this._emitFeatureUpdate(featureId, "remote-defaults-update");
  }

  deleteRemoteConfig(featureId) {
    const remoteConfigState = this.get(REMOTE_DEFAULTS_KEY);
    delete remoteConfigState?.[featureId];
    this.setNonPersistent(REMOTE_DEFAULTS_KEY, { ...remoteConfigState });
    syncDataStore.deleteDefault(featureId);
    this._emitFeatureUpdate(featureId, "remote-defaults-update");
  }

  /**
   * Query the store for the remote configuration of a feature
   * @param {string} featureId The feature we want to query for
   * @returns {{RemoteDefaults}|undefined} Remote defaults if available
   */
  getRemoteConfig(featureId) {
    return (
      this.get(REMOTE_DEFAULTS_KEY)?.[featureId] ||
      syncDataStore.getDefault(featureId)
    );
  }

  /**
   * Get all existing active remote config ids
   * @returns {Array<string>}
   */
  getAllExistingRemoteConfigIds() {
    return [
      ...new Set([
        ...syncDataStore.getAllDefaultBranches(),
        ...Object.keys(this.get(REMOTE_DEFAULTS_KEY) || {}),
      ]),
    ];
  }

  getAllRemoteConfigs() {
    const remoteDefaults = this.get(REMOTE_DEFAULTS_KEY);
    if (!remoteDefaults) {
      return [];
    }

    let featureIds = Object.keys(remoteDefaults);
    return Object.values(remoteDefaults).map((rc, idx) => ({
      ...rc,
      featureId: featureIds[idx],
    }));
  }

  _deleteForTests(featureId) {
    super._deleteForTests(featureId);
    syncDataStore.deleteDefault(featureId);
    syncDataStore.delete(featureId);
  }
}
