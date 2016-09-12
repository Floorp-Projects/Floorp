/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

Cu.import("chrome://marionette/content/error.js");

this.EXPORTED_SYMBOLS = ["addon"];

this.addon = {};

/**
 * Installs Firefox addon.
 *
 * If the addon is restartless, it can be used right away. Otherwise a
 * restart is needed.
 *
 * Temporary addons will automatically be unisntalled on shutdown and
 * do not need to be signed, though they must be restartless.
 *
 * @param {string} path
 *     Full path to the extension package archive to be installed.
 * @param {boolean=} temporary
 *     Install the add-on temporarily if true.
 *
 * @return {Promise.<string>}
 *     Addon ID string of the newly installed addon.
 *
 * @throws {AddonError}
 *     if installation fails
 */
addon.install = function(path, temporary = false) {
  return new Promise((resolve, reject) => {
    let listener = {
      onInstallEnded: function(install, addon) {
        resolve(addon.id);
      },

      onInstallFailed: function(install) {
        reject(new AddonError(install.error));
      },

      onInstalled: function(addon) {
        AddonManager.removeAddonListener(listener);
        resolve(addon.id);
      }
    };

    let file = new FileUtils.File(path);

    // temporary addons
    if (temp) {
      AddonManager.addAddonListener(listener);
      AddonManager.installTemporaryAddon(file);
    }

    // addons that require restart
    else {
      AddonManager.getInstallForFile(file, function(aInstall) {
        if (aInstall.error != 0) {
          reject(new AddonError(aInstall.error));
        }
        aInstall.addListener(listener);
        aInstall.install();
      });
    }
  });
};

/**
 * Uninstall a Firefox addon.
 *
 * If the addon is restartless, it will be uninstalled right
 * away. Otherwise a restart is necessary.
 *
 * @param {string} id
 *     Addon ID to uninstall.
 *
 * @return {Promise}
 */
addon.uninstall = function(id) {
  return new Promise(resolve => {
    AddonManager.getAddonByID(arguments[0], function(addon) {
      addon.uninstall();
    });
  });
};
