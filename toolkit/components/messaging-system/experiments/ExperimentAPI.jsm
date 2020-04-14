/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ExperimentAPI"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ExperimentStore:
    "resource://messaging-system/experiments/ExperimentStore.jsm",
});

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
};

XPCOMUtils.defineLazyGetter(ExperimentAPI, "_store", function() {
  return new ExperimentStore();
});
