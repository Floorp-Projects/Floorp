/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ExperimentStore"];

const { SharedDataMap } = ChromeUtils.import(
  "resource://nimbus/lib/SharedDataMap.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  FeatureManifest: "resource://nimbus/FeatureManifest.js",
  PrefUtils: "resource://normandy/lib/PrefUtils.jsm",
});

const IS_MAIN_PROCESS =
  Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

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
XPCOMUtils.defineLazyGetter(lazy, "syncDataStore", () => {
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
        let value = lazy.PrefUtils.getPref(prefName);
        // Try to parse string values that could be stringified objects
        if (
          lazy.FeatureManifest[featureId]?.variables[childPref]?.type === "json"
        ) {
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
      metadata.branch.feature.value = this._getBranchChildValues(
        prefBranch,
        featureId
      );

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
    setDefault(featureId, enrollment) {
      /* We store configuration variables separately in pref branches of
       * appropriate type:
       * (feature: "foo") { variables: { enabled: true } }
       * gets stored as `${SYNC_DEFAULTS_PREF_BRANCH}foo.enabled=true`
       */
      let { feature } = enrollment.branch;
      for (let variable of Object.keys(feature.value)) {
        let prefName = `${SYNC_DEFAULTS_PREF_BRANCH}${featureId}.${variable}`;
        this._trySetTypedPrefValue(prefName, feature.value[variable]);
      }
      this._trySetPrefValue(defaultsPrefBranch, featureId, {
        ...enrollment,
        branch: {
          ...enrollment.branch,
          feature: {
            ...enrollment.branch.feature,
            value: null,
          },
        },
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

/**
 * Returns all feature ids associated with the branch provided.
 * Fallback for when `featureIds` was not persisted to disk. Can be removed
 * after bug 1725240 has reached release.
 *
 * @param {Branch} branch
 * @returns {string[]}
 */
function getAllBranchFeatureIds(branch) {
  return featuresCompat(branch).map(f => f.featureId);
}

function featuresCompat(branch) {
  if (!branch || (!branch.feature && !branch.features)) {
    return [];
  }
  let { features } = branch;
  // In <=v1.5.0 of the Nimbus API, experiments had single feature
  if (!features) {
    features = [branch.feature];
  }

  return features;
}

class ExperimentStore extends SharedDataMap {
  static SYNC_DATA_PREF_BRANCH = SYNC_DATA_PREF_BRANCH;
  static SYNC_DEFAULTS_PREF_BRANCH = SYNC_DEFAULTS_PREF_BRANCH;

  constructor(sharedDataKey, options = { isParent: IS_MAIN_PROCESS }) {
    super(sharedDataKey || DEFAULT_STORE_ID, options);
  }

  async init() {
    await super.init();

    this.getAllActive().forEach(({ branch, featureIds }) => {
      (featureIds || getAllBranchFeatureIds(branch)).forEach(featureId =>
        this._emitFeatureUpdate(featureId, "feature-experiment-loaded")
      );
    });
    this.getAllRollouts().forEach(({ featureIds }) => {
      featureIds.forEach(featureId =>
        this._emitFeatureUpdate(featureId, "feature-rollout-loaded")
      );
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
          getAllBranchFeatureIds(experiment.branch).includes(featureId)
        // Default to the pref store if data is not yet ready
      ) || lazy.syncDataStore.get(featureId)
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
    return this.getAll().filter(
      enrollment => enrollment.active && !enrollment.isRollout
    );
  }

  /**
   * Returns all active rollouts
   * @returns {array}
   */
  getAllRollouts() {
    return this.getAll().filter(
      enrollment => enrollment.active && enrollment.isRollout
    );
  }

  /**
   * Query the store for the remote configuration of a feature
   * @param {string} featureId The feature we want to query for
   * @returns {{Rollout}|undefined} Remote defaults if available
   */
  getRolloutForFeature(featureId) {
    return (
      this.getAllRollouts().find(r => r.featureIds.includes(featureId)) ||
      lazy.syncDataStore.getDefault(featureId)
    );
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

  _emitUpdates(enrollment) {
    this.emit(`update:${enrollment.slug}`, enrollment);
    (
      enrollment.featureIds || getAllBranchFeatureIds(enrollment.branch)
    ).forEach(featureId => {
      this.emit(`update:${featureId}`, enrollment);
      this._emitFeatureUpdate(
        featureId,
        enrollment.isRollout ? "rollout-updated" : "experiment-updated"
      );
    });
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
   * Persists early startup experiments or rollouts
   * @param {Enrollment} enrollment Experiment or rollout
   */
  _updateSyncStore(enrollment) {
    let features = featuresCompat(enrollment.branch);
    for (let feature of features) {
      if (
        lazy.FeatureManifest[feature.featureId]?.isEarlyStartup ||
        feature.isEarlyStartup
      ) {
        if (!enrollment.active) {
          // Remove experiments on un-enroll, no need to check if it exists
          if (enrollment.isRollout) {
            lazy.syncDataStore.deleteDefault(feature.featureId);
          } else {
            lazy.syncDataStore.delete(feature.featureId);
          }
        } else {
          let updateEnrollmentSyncStore = enrollment.isRollout
            ? lazy.syncDataStore.setDefault.bind(lazy.syncDataStore)
            : lazy.syncDataStore.set.bind(lazy.syncDataStore);
          updateEnrollmentSyncStore(feature.featureId, {
            ...enrollment,
            branch: {
              ...enrollment.branch,
              feature,
              // Only store the early startup feature
              features: null,
            },
          });
        }
      }
    }
  }

  /**
   * Add an enrollment and notify listeners
   * @param {Enrollment} enrollment
   */
  addEnrollment(enrollment) {
    if (!enrollment || !enrollment.slug) {
      throw new Error(
        `Tried to add an experiment but it didn't have a .slug property.`
      );
    }

    this.set(enrollment.slug, enrollment);
    this._updateSyncStore(enrollment);
    this._emitUpdates(enrollment);
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
        `Tried to update experiment ${slug} but it doesn't exist`
      );
    }
    const updatedExperiment = { ...oldProperties, ...newProperties };
    this.set(slug, updatedExperiment);
    this._updateSyncStore(updatedExperiment);
    this._emitUpdates(updatedExperiment);
  }

  /**
   * Test only helper for cleanup
   *
   * @param slugOrFeatureId Can be called with slug (which removes the SharedDataMap entry) or
   * with featureId which removes the SyncDataStore entry for the feature
   */
  _deleteForTests(slugOrFeatureId) {
    super._deleteForTests(slugOrFeatureId);
    lazy.syncDataStore.deleteDefault(slugOrFeatureId);
    lazy.syncDataStore.delete(slugOrFeatureId);
  }
}
