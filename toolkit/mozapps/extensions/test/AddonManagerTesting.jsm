/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is a test-only JSM containing utility methods for
// interacting with the add-ons manager.

"use strict";

var EXPORTED_SYMBOLS = [
  "AddonManagerTesting",
];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");

var AddonManagerTesting = {
  /**
   * Get the add-on that is specified by its ID.
   *
   * @return {Promise<Object>} A promise that resolves returning the found addon or null
   *         if it is not found.
   */
  getAddonById(id) {
    return AddonManager.getAddonByID(id);
  },

  /**
   * Uninstall an add-on that is specified by its ID.
   *
   * The returned promise resolves on successful uninstall and rejects
   * if the add-on is not unknown.
   *
   * @return Promise<restartRequired>
   */
  uninstallAddonByID(id) {
    return new Promise(async (resolve, reject) => {

      let addon = await AddonManager.getAddonByID(id);
      if (!addon) {
        reject(new Error("Add-on is not known: " + id));
        return;
      }

      let listener = {
        onUninstalling(addon, needsRestart) {
          if (addon.id != id) {
            return;
          }

          if (needsRestart) {
            AddonManager.removeAddonListener(listener);
            resolve(true);
          }
        },

        onUninstalled(addon) {
          if (addon.id != id) {
            return;
          }

          AddonManager.removeAddonListener(listener);
          resolve(false);
        },

        onOperationCancelled(addon) {
          if (addon.id != id) {
            return;
          }

          AddonManager.removeAddonListener(listener);
          reject(new Error("Uninstall cancelled."));
        },
      };

      AddonManager.addAddonListener(listener);
      addon.uninstall();

    });
  },

  /**
   * Install an XPI add-on from a URL.
   *
   * @return Promise<addon>
   */
  installXPIFromURL(url, hash, name, iconURL, version) {
    return new Promise(async (resolve, reject) => {

      let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", hash, name, iconURL, version);
      let fail = () => { reject(new Error("Add-on install failed.")); };

      let listener = {
        onDownloadCancelled: fail,
        onDownloadFailed: fail,
        onInstallCancelled: fail,
        onInstallFailed: fail,
        onInstallEnded(install, addon) {
          resolve(addon);
        },
      };

      install.addListener(listener);
      install.install();

    });
  },
};
