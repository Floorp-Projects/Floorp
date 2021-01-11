/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * @typedef {import("./@types/ExperimentManager").Enrollment} Enrollment
 * @typedef {import("./@types/ExperimentManager").FeatureConfig} FeatureConfig
 */

const EXPORTED_SYMBOLS = ["ExperimentStore"];

const { SharedDataMap } = ChromeUtils.import(
  "resource://messaging-system/lib/SharedDataMap.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const SYNC_DATA_PREF = "messaging-system.syncdatastore.data";
let tryJSONParse = data => {
  try {
    return JSON.parse(data);
  } catch (e) {}

  return {};
};
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "syncDataStore",
  SYNC_DATA_PREF,
  {},
  // aOnUpdate
  (data, prev, latest) => tryJSONParse(latest),
  // aTransform
  tryJSONParse
);

const DEFAULT_STORE_ID = "ExperimentStoreData";
// Experiment feature configs that should be saved to prefs for
// fast access on startup.
const SYNC_ACCESS_FEATURES = ["newtab", "aboutwelcome"];

class ExperimentStore extends SharedDataMap {
  constructor(sharedDataKey, options) {
    super(sharedDataKey || DEFAULT_STORE_ID, options);
  }

  /**
   * Given a feature identifier, find an active experiment that matches that feature identifier.
   * This assumes, for now, that there is only one active experiment per feature per browser.
   *
   * @param {string} featureId
   * @returns {Enrollment|undefined} An active experiment if it exists
   * @memberof ExperimentStore
   */
  getExperimentForFeature(featureId) {
    return this.getAllActive().find(
      experiment => experiment.branch.feature?.featureId === featureId
    );
  }

  /**
   * Return FeatureConfig from first active experiment where it can be found
   * @param {{slug: string, featureId: string, sendExposurePing: bool}}
   * @returns {Branch | null}
   */
  activateBranch({ slug, featureId, sendExposurePing = true }) {
    for (let experiment of this.getAllActive()) {
      if (
        experiment?.branch.feature.featureId === featureId ||
        experiment.slug === slug
      ) {
        if (sendExposurePing) {
          this._emitExperimentExposure({
            experimentSlug: experiment.slug,
            branchSlug: experiment.branch.slug,
            featureId,
          });
        }
        // Default to null for feature-less experiments where we're only
        // interested in exposure.
        return experiment?.branch || null;
      }
    }

    return null;
  }

  /**
   * Check if an active experiment already exists for a feature
   *
   * @param {string} featureId
   * @returns {boolean} Does an active experiment exist for that feature?
   * @memberof ExperimentStore
   */
  hasExperimentForFeature(featureId) {
    if (!featureId) {
      return false;
    }
    if (this.activateBranch({ featureId })?.feature.featureId === featureId) {
      return true;
    }
    return false;
  }

  /**
   * @returns {Enrollment[]}
   */
  getAll() {
    if (!this._data) {
      return Object.values(syncDataStore);
    }

    return Object.values(this._data);
  }

  /**
   * @returns {Enrollment[]}
   */
  getAllActive() {
    return this.getAll().filter(experiment => experiment.active);
  }

  _emitExperimentUpdates(experiment) {
    this.emit(`update:${experiment.slug}`, experiment);
    if (experiment.branch.feature) {
      this.emit(`update:${experiment.branch.feature.featureId}`, experiment);
    }
  }

  /**
   * @param {{featureId: string, experimentSlug: string, branchSlug: string}} experimentData
   */
  _emitExperimentExposure(experimentData) {
    this.emit("exposure", experimentData);
  }

  /**
   * @param {Enrollment} experiment
   */
  _updateSyncStore(experiment) {
    if (SYNC_ACCESS_FEATURES.includes(experiment.branch.feature?.featureId)) {
      if (!experiment.active) {
        // Remove experiments on un-enroll, otherwise nothing to do
        if (syncDataStore[experiment.slug]) {
          delete syncDataStore[experiment.slug];
        }
      } else {
        syncDataStore[experiment.slug] = experiment;
      }
      Services.prefs.setStringPref(
        SYNC_DATA_PREF,
        JSON.stringify(syncDataStore)
      );
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
    this._emitExperimentUpdates(experiment);
    this._updateSyncStore(experiment);
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
    this._emitExperimentUpdates(updatedExperiment);
    this._updateSyncStore(updatedExperiment);
  }
}
