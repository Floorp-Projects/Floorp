/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["PrefUtils"];

const kPrefBranches = {
  user: Services.prefs,
  default: Services.prefs.getDefaultBranch(""),
};

var PrefUtils = {
  /**
   * Get a preference from the named branch
   * @param {string} branchName One of "default" or "user"
   * @param {string} pref
   * @param {string|boolean|integer|null} [default]
   *   The value to return if the preference does not exist. Defaults to null.
   */
  getPref(branchName, pref, defaultValue = null) {
    const branch = kPrefBranches[branchName];
    const type = branch.getPrefType(pref);

    try {
      switch (type) {
        case Services.prefs.PREF_BOOL: {
          return branch.getBoolPref(pref);
        }
        case Services.prefs.PREF_STRING: {
          return branch.getStringPref(pref);
        }
        case Services.prefs.PREF_INT: {
          return branch.getIntPref(pref);
        }
        case Services.prefs.PREF_INVALID: {
          return defaultValue;
        }
      }
    } catch (e) {
      if (branchName === "default" && e.result === Cr.NS_ERROR_UNEXPECTED) {
        // There is a value for the pref on the user branch but not on the default branch. This is ok.
        return defaultValue;
      }
      // Unexpected error, re-throw it
      throw e;
    }

    // If `type` isn't any of the above, throw an error. Don't do this in a
    // default branch of switch so that error handling is easier.
    throw new TypeError(`Unknown preference type (${type}) for ${pref}.`);
  },

  /**
   * Set a preference on the named branch
   * @param {string} branchName One of "default" or "user"
   * @param {string} pref
   * @param {string|boolean|integer|null} value
   *   The value to set. Must match the type named in `type`.
   */
  setPref(branchName, pref, value) {
    if (value === null) {
      this.clearPref(branchName, pref);
      return;
    }
    const branch = kPrefBranches[branchName];
    switch (typeof value) {
      case "boolean": {
        branch.setBoolPref(pref, value);
        break;
      }
      case "string": {
        branch.setStringPref(pref, value);
        break;
      }
      case "number": {
        branch.setIntPref(pref, value);
        break;
      }
      default: {
        throw new TypeError(`Unexpected value type (${typeof value}) for ${pref}.`);
      }
    }
  },

  /**
   * Remove a preference from a branch.
   * @param {string} branchName One of "default" or "user"
   * @param {string} pref
   */
  clearPref(branchName, pref) {
    if (branchName === "user") {
      kPrefBranches.user.clearUserPref(pref);
    } else if (branchName === "default") {
      // deleteBranch will affect the user branch as well. Get the user-branch
      // value, and re-set it after clearing the pref.
      const hadUserValue = Services.prefs.prefHasUserValue(pref);
      const originalValue = this.getPref("user", pref, null);
      kPrefBranches.default.deleteBranch(pref);
      if (hadUserValue) {
        this.setPref(branchName, pref, originalValue);
      }
    }
  }
};
