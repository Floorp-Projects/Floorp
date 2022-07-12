/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["FormHistoryTestUtils"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.jsm",
});

/**
 * Provides a js-friendly promise-based API around FormHistory, and utils.
 *
 * @note This is not a 100% complete implementation, it is intended for quick
 * additions and check, thus further changes may be necessary for different
 * use-cases.
 */
var FormHistoryTestUtils = {
  makeListener(resolve, reject) {
    let results = [];
    return {
      _results: results,
      handleResult(result) {
        results.push(result);
      },
      handleError(error) {
        reject(error);
      },
      handleCompletion(errored) {
        if (!errored) {
          resolve(results);
        }
      },
    };
  },

  /**
   * Adds values to form history.
   *
   * @param {string} fieldname The field name.
   * @param {array} additions Array of entries describing the values to add.
   *   Each entry can either be a string, or an object with the shape
   *   { value, source}.
   * @returns {Promise} Resolved once the operation is complete.
   */
  async add(fieldname, additions = []) {
    // Additions are made one by one, so multiple identical entries are properly
    // applied.
    additions = additions.map(v => (typeof v == "string" ? { value: v } : v));
    for (let { value, source } of additions) {
      await new Promise((resolve, reject) => {
        lazy.FormHistory.update(
          Object.assign({ fieldname }, { op: "bump", value, source }),
          this.makeListener(resolve, reject)
        );
      });
    }
  },

  /**
   * Counts values from form history.
   *
   * @param {string} fieldname The field name.
   * @param {array} filters Objects describing the search properties.
   * @returns {number} The number of entries found.
   */
  async count(fieldname, filters = {}) {
    let results = await new Promise((resolve, reject) => {
      lazy.FormHistory.count(
        Object.assign({ fieldname }, filters),
        this.makeListener(resolve, reject)
      );
    });
    return results[0];
  },

  /**
   * Removes values from form history.
   * If you want to remove all history, use clear() instead.
   *
   * @param {string} fieldname The field name.
   * @param {array} removals Array of entries describing the values to add.
   *   Each entry can either be a string, or an object with the shape
   *   { value, source}. If source is specified, only the source relation will
   *   be removed, while the global form history value persists.
   * @param {object} window The window containing the urlbar.
   * @returns {Promise} Resolved once the operation is complete.
   */
  remove(fieldname, removals) {
    let changes = removals.map(v => {
      let criteria = typeof v == "string" ? { value: v } : v;
      return Object.assign({ fieldname, op: "remove" }, criteria);
    });
    return new Promise((resolve, reject) => {
      lazy.FormHistory.update(changes, this.makeListener(resolve, reject));
    });
  },

  /**
   * Removes all values from form history.
   * If you want to remove individual values, use remove() instead.
   *
   * @param {string} fieldname The field name whose history should be cleared.
   *   Can be omitted to clear all form history.
   * @returns {Promise} Resolved once the operation is complete.
   */
  clear(fieldname) {
    return new Promise((resolve, reject) => {
      let baseChange = fieldname ? { fieldname } : {};
      lazy.FormHistory.update(
        Object.assign(baseChange, { op: "remove" }),
        this.makeListener(resolve, reject)
      );
    });
  },

  /**
   * Searches form history.
   *
   * @param {string} fieldname The field name.
   * @param {array} filters Objects describing the search properties.
   * @returns {Promise} Resolved once the operation is complete.
   * @resolves {Array} Array of found form history entries.
   */
  search(fieldname, filters = {}) {
    return new Promise((resolve, reject) => {
      lazy.FormHistory.search(
        null,
        Object.assign({ fieldname }, filters),
        this.makeListener(resolve, reject)
      );
    });
  },

  /**
   * Gets autocomplete results from form history.
   *
   * @param {string} searchString The search string.
   * @param {string} fieldname The field name.
   * @param {array} filters Objects describing the search properties.
   * @returns {Promise} Resolved once the operation is complete.
   * @resolves {Array} Array of found form history entries.
   */
  autocomplete(searchString, fieldname, filters = {}) {
    return new Promise((resolve, reject) => {
      lazy.FormHistory.getAutoCompleteResults(
        searchString,
        Object.assign({ fieldname }, filters),
        this.makeListener(resolve, reject)
      );
    });
  },
};
