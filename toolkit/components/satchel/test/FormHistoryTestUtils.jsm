/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["FormHistoryTestUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
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
   * @param {array} additions Array of objects describing the values to add,
   *   each object must have a value property and can have a source property.
   * @returns {Promise} Resolved once the operation is complete.
   */
  add(fieldname, additions = []) {
    let promises = [];
    for (let { value, source } of additions) {
      promises.push(
        new Promise((resolve, reject) => {
          FormHistory.update(
            Object.assign({ fieldname }, { op: "bump", value, source }),
            this.makeListener(resolve, reject)
          );
        })
      );
    }
    return Promise.all(promises);
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
      FormHistory.count(
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
   * @param {array} removals Array of objects describing the values to remove,
   *   each object must have a value property and can have a source property.
   *   If source is specified, only the source relation will be removed, while
   *   the global form history value persists.
   * @param {object} window The window containing the urlbar.
   * @returns {Promise} Resolved once the operation is complete.
   */
  remove(fieldname, removals) {
    let promises = [];
    for (let { value, source } of removals) {
      promises.push(
        new Promise((resolve, reject) => {
          FormHistory.update(
            Object.assign({ fieldname }, { op: "remove", value, source }),
            this.makeListener(resolve, reject)
          );
        })
      );
    }
    return Promise.all(promises);
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
      FormHistory.update(
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
      FormHistory.search(
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
      FormHistory.getAutoCompleteResults(
        searchString,
        Object.assign({ fieldname }, filters),
        this.makeListener(resolve, reject)
      );
    });
  },
};
