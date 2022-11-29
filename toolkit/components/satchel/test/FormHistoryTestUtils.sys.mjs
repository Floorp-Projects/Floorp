/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
});

/**
 * Provides a js-friendly promise-based API around FormHistory, and utils.
 *
 * Note: This is not a 100% complete implementation, it is intended for quick
 * additions and check, thus further changes may be necessary for different
 * use-cases.
 */
export var FormHistoryTestUtils = {
  /**
   * Adds values to form history.
   *
   * @param {string} fieldname The field name.
   * @param {Array} additions Array of entries describing the values to add.
   *   Each entry can either be a string, or an object with the shape
   *   { value, source}.
   * @returns {Promise} Resolved once the operation is complete.
   */
  async add(fieldname, additions = []) {
    // Additions are made one by one, so multiple identical entries are properly
    // applied.
    additions = additions.map(v => (typeof v == "string" ? { value: v } : v));
    for (let { value, source } of additions) {
      await lazy.FormHistory.update(
        Object.assign({ fieldname }, { op: "bump", value, source })
      );
    }
  },

  /**
   * Counts values from form history.
   *
   * @param {string} fieldname The field name.
   * @param {Array} filters Objects describing the search properties.
   * @returns {number} The number of entries found.
   */
  async count(fieldname, filters = {}) {
    return lazy.FormHistory.count(Object.assign({ fieldname }, filters));
  },

  /**
   * Removes values from form history.
   * If you want to remove all history, use clear() instead.
   *
   * @param {string} fieldname The field name.
   * @param {Array} removals Array of entries describing the values to add.
   *   Each entry can either be a string, or an object with the shape
   *   { value, source}. If source is specified, only the source relation will
   *   be removed, while the global form history value persists.
   * @returns {Promise} Resolved once the operation is complete.
   */
  remove(fieldname, removals) {
    let changes = removals.map(v => {
      let criteria = typeof v == "string" ? { value: v } : v;
      return Object.assign({ fieldname, op: "remove" }, criteria);
    });
    return lazy.FormHistory.update(changes);
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
    let baseChange = fieldname ? { fieldname } : {};
    return lazy.FormHistory.update(Object.assign(baseChange, { op: "remove" }));
  },

  /**
   * Searches form history.
   *
   * @param {string} fieldname The field name.
   * @param {Array} filters Objects describing the search properties.
   * @returns {Promise<Array>} Resolves an array of found form history entries.
   */
  search(fieldname, filters = {}) {
    return lazy.FormHistory.search(null, Object.assign({ fieldname }, filters));
  },

  /**
   * Gets autocomplete results from form history.
   *
   * @param {string} searchString The search string.
   * @param {string} fieldname The field name.
   * @param {Array} filters Objects describing the search properties.
   * @returns {Promise<Array>} Resolves an array of found form history entries.
   */
  autocomplete(searchString, fieldname, filters = {}) {
    return lazy.FormHistory.getAutoCompleteResults(
      searchString,
      Object.assign({ fieldname }, filters)
    );
  },
};
