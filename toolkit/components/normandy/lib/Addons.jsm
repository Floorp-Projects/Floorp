/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");

var EXPORTED_SYMBOLS = ["Addons"];

/**
 * SafeAddons store info about an add-on. They are single-depth
 * objects to simplify cloning, and have no methods so they are safe
 * to pass to sandboxes and filter expressions.
 *
 * @typedef {Object} SafeAddon
 * @property {string} id
 *   Add-on id, such as "shield-recipe-client@mozilla.com" or "{4ea51ac2-adf2-4af8-a69d-17b48c558a12}"
 * @property {Date} installDate
 * @property {boolean} isActive
 * @property {string} name
 * @property {string} type
 *   "extension", "theme", etc.
 * @property {string} version
 */

var Addons = {
  /**
   * Get information about an installed add-on by ID.
   *
   * @param {string} addonId
   * @returns {SafeAddon?} Add-on with given ID, or null if not found.
   * @throws If addonId is not specified or not a string.
   */
  async get(addonId) {
    const addon = await AddonManager.getAddonByID(addonId);
    if (!addon) {
      return null;
    }
    return this.serializeForSandbox(addon);
  },

  /**
   * Installs an add-on
   *
   * @param {string} addonUrl
   *   Url to download the .xpi for the add-on from.
   * @param {object} options
   * @param {boolean} options.update=false
   *   If true, will update an existing installed add-on with the same ID.
   * @async
   * @returns {string}
   *   Add-on ID that was installed
   * @throws {string}
   *   If the add-on can not be installed, or overwriting is disabled and an
   *   add-on with a matching ID is already installed.
   */
  async install(addonUrl, options) {
    const installObj = await AddonManager.getInstallForURL(addonUrl, "application/x-xpinstall");
    return this.applyInstall(installObj, options);
  },

  async applyInstall(addonInstall, {update = false} = {}) {
    const result = new Promise((resolve, reject) => addonInstall.addListener({
      onInstallStarted(cbInstall) {
        if (cbInstall.existingAddon && !update) {
          reject(new Error(`
            Cannot install add-on ${cbInstall.addon.id}; an existing add-on
            with the same ID exists and updating is disabled.
          `));
          return false;
        }
        return true;
      },
      onInstallEnded(cbInstall, addon) {
        resolve(addon.id);
      },
      onInstallFailed(cbInstall) {
        reject(new Error(`AddonInstall error code: [${cbInstall.error}]`));
      },
      onDownloadFailed(cbInstall) {
        reject(new Error(`Download failed: [${cbInstall.sourceURI.spec}]`));
      },
    }));
    addonInstall.install();
    return result;
  },

  /**
   * Uninstalls an add-on by ID.
   * @param addonId {string} Add-on ID to uninstall.
   * @async
   * @throws If no add-on with `addonId` is installed.
   */
  async uninstall(addonId) {
    const addon = await AddonManager.getAddonByID(addonId);
    if (addon === null) {
      throw new Error(`No addon with ID [${addonId}] found.`);
    }
    addon.uninstall();
    return null;
  },

  /**
   * Make a safe serialization of an add-on
   * @param addon {Object} An add-on object as returned from AddonManager.
   */
  serializeForSandbox(addon) {
    return {
      id: addon.id,
      installDate: new Date(addon.installDate),
      isActive: addon.isActive,
      name: addon.name,
      type: addon.type,
      version: addon.version,
    };
  },
};
