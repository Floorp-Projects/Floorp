/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * @typedef {import("../@types/ExperimentManager").Enrollment} Enrollment
 */

const EXPORTED_SYMBOLS = ["ExperimentStore"];

const { SharedDataMap } = ChromeUtils.import(
  "resource://messaging-system/lib/SharedDataMap.jsm"
);

const DEFAULT_STORE_ID = "ExperimentStoreData";

class ExperimentStore extends SharedDataMap {
  constructor(sharedDataKey) {
    super(sharedDataKey || DEFAULT_STORE_ID);
  }

  /**
   * Given a group identifier, find an active experiment that matches that group identifier.
   * For example, getExperimentForGroup("B") would return an experiment with groups ["A", "B", "C"]
   * This assumes, for now, that there is only one active experiment per group per browser.
   *
   * @param {string} group
   * @returns {Enrollment|undefined} An active experiment if it exists
   * @memberof ExperimentStore
   */
  getExperimentForGroup(group) {
    for (const [, experiment] of this._map) {
      if (experiment.active && experiment.branch.groups?.includes(group)) {
        return experiment;
      }
    }
    return undefined;
  }

  /**
   * Check if an active experiment already exists for a set of groups
   *
   * @param {Array<string>} groups
   * @returns {boolean} Does an active experiment exist for that group?
   * @memberof ExperimentStore
   */
  hasExperimentForGroups(groups) {
    if (!groups || !groups.length) {
      return false;
    }
    for (const [, experiment] of this._map) {
      if (
        experiment.active &&
        experiment.branch.groups?.filter(g => groups.includes(g)).length
      ) {
        return true;
      }
    }
    return false;
  }

  /**
   * @returns {Enrollment[]}
   */
  getAll() {
    return [...this._map.values()];
  }

  /**
   * @returns {Enrollment[]}
   */
  getAllActive() {
    return this.getAll().filter(experiment => experiment.active);
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
    this.set(slug, { ...oldProperties, ...newProperties });
  }
}
