/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is a test-only JSM containing utility methods for
// interacting with the add-ons manager.

"use strict";

this.EXPORTED_SYMBOLS = [
  "AddonManagerTesting",
];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

this.AddonManagerTesting = {
  /**
   * Get the add-on that is specified by its ID.
   *
   * @return {Promise<Object>} A promise that resolves returning the found addon or null
   *         if it is not found.
   */
  getAddonById(id) {
    return new Promise(resolve => AddonManager.getAddonByID(id, addon => resolve(addon)));
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
    let deferred = Promise.defer();

    AddonManager.getAddonByID(id, (addon) => {
      if (!addon) {
        deferred.reject(new Error("Add-on is not known: " + id));
        return;
      }

      let listener = {
        onUninstalling(addon, needsRestart) {
          if (addon.id != id) {
            return;
          }

          if (needsRestart) {
            AddonManager.removeAddonListener(listener);
            deferred.resolve(true);
          }
        },

        onUninstalled(addon) {
          if (addon.id != id) {
            return;
          }

          AddonManager.removeAddonListener(listener);
          deferred.resolve(false);
        },

        onOperationCancelled(addon) {
          if (addon.id != id) {
            return;
          }

          AddonManager.removeAddonListener(listener);
          deferred.reject(new Error("Uninstall cancelled."));
        },
      };

      AddonManager.addAddonListener(listener);
      addon.uninstall();
    });

    return deferred.promise;
  },

  /**
   * Install an XPI add-on from a URL.
   *
   * @return Promise<addon>
   */
  installXPIFromURL(url, hash, name, iconURL, version) {
    let deferred = Promise.defer();

    AddonManager.getInstallForURL(url, (install) => {
      let fail = () => { deferred.reject(new Error("Add-on install failed.")) };

      let listener = {
        onDownloadCancelled: fail,
        onDownloadFailed: fail,
        onInstallCancelled: fail,
        onInstallFailed: fail,
        onInstallEnded(install, addon) {
          deferred.resolve(addon);
        },
      };

      install.addListener(listener);
      install.install();
    }, "application/x-xpinstall", hash, name, iconURL, version);

    return deferred.promise;
  },
};
