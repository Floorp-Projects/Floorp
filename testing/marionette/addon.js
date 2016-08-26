/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

Cu.import("chrome://marionette/content/error.js");

this.EXPORTED_SYMBOLS = ["addon"];

this.addon = {};

// from https://developer.mozilla.org/en-US/Add-ons/Add-on_Manager/AddonManager#AddonInstall_errors
addon.Errors = {
  [-1]: "ERROR_NETWORK_FAILURE: A network error occured.",
  [-2]: "ERROR_INCORECT_HASH: The downloaded file did not match the expected hash.",
  [-3]: "ERROR_CORRUPT_FILE: The file appears to be corrupt.",
  [-4]: "ERROR_FILE_ACCESS: There was an error accessing the filesystem.",
  [-5]: "ERROR_SIGNEDSTATE_REQUIRED: The addon must be signed and isn't.",
};

function lookupError(code) {
  let msg = addon.Errors[code];
  return new UnknownError(msg);
}

/**
 * Install a Firefox addon.
 *
 * If the addon is restartless, it can be used right away.  Otherwise a
 * restart is required.
 *
 * Temporary addons will automatically be uninstalled on shutdown and
 * do not need to be signed, though they must be restartless.
 *
 * @param {string} path
 *     Full path to the extension package archive.
 * @param {boolean=} temporary
 *     True to install the addon temporarily, false (default) otherwise.
 *
 * @return {Promise: string}
 *     Addon ID.
 *
 * @throws {UnknownError}
 *     If there is a problem installing the addon.
 */
addon.install = function(path, temporary = false) {
  return new Promise((resolve, reject) => {
    let file = new FileUtils.File(path);

    let listener = {
      onInstallEnded: function(install, addon) {
        resolve(addon.id);
      },

      onInstallFailed: function(install) {
        reject(lookupError(install.error));
      },

      onInstalled: function(addon) {
        AddonManager.removeAddonListener(listener);
        resolve(addon.id);
      }
    };

    if (!temporary) {
      AddonManager.getInstallForFile(file, function(aInstall) {
        if (aInstall.error !== 0) {
          reject(lookupError(aInstall.error));
        }
        aInstall.addListener(listener);
        aInstall.install();
      });
    } else {
      AddonManager.addAddonListener(listener);
      AddonManager.installTemporaryAddon(file);
    }
  });
};

/**
 * Uninstall a Firefox addon.
 *
 * If the addon is restartless it will be uninstalled right away.
 * Otherwise, Firefox must be restarted for the change to take effect.
 *
 * @param {string} id
 *     ID of the addon to uninstall.
 *
 * @return {Promise}
 */
addon.uninstall = function(id) {
  return new Promise(resolve => {
    AddonManager.getAddonByID(id, function(addon) {
      addon.uninstall();
      resolve();
    });
  });
};
