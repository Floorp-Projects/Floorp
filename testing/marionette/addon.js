/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

const {UnknownError} = Cu.import("chrome://marionette/content/error.js", {});

this.EXPORTED_SYMBOLS = ["addon"];

/** @namespace */
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

async function installAddon(file) {
  let install = await AddonManager.getInstallForFile(file);

  return new Promise((resolve, reject) => {
    if (install.error != 0) {
      reject(new UnknownError(lookupError(install.error)));
    }

    let addonId = install.addon.id;

    let success = install => {
      if (install.addon.id === addonId) {
        install.removeListener(listener);
        resolve(install.addon);
      }
    };

    let fail = install => {
      if (install.addon.id === addonId) {
        install.removeListener(listener);
        reject(new UnknownError(lookupError(install.error)));
      }
    };

    let listener = {
      onDownloadCancelled: fail,
      onDownloadFailed: fail,
      onInstallCancelled: fail,
      onInstallFailed: fail,
      onInstallEnded: success,
    };

    install.addListener(listener);
    install.install();
  });
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
 * @return {Promise.<string>}
 *     Addon ID.
 *
 * @throws {UnknownError}
 *     If there is a problem installing the addon.
 */
addon.install = async function(path, temporary = false) {
  let file = new FileUtils.File(path);
  let addon;

  if (!file.exists()) {
    throw new UnknownError(`Could not find add-on at '${path}'`);
  }

  try {
    if (temporary) {
      addon = await AddonManager.installTemporaryAddon(file);
    } else {
      addon = await installAddon(file);
    }
  } catch (e) {
    throw new UnknownError(
        `Could not install add-on at '${path}': ${e.message}`);
  }

  return addon.id;
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
 *
 * @throws {UnknownError}
 *     If there is a problem uninstalling the addon.
 */
addon.uninstall = async function(id) {
  return AddonManager.getAddonByID(id).then(addon => {
    let listener = {
      onOperationCancelled: addon => {
        if (addon.id === id) {
          AddonManager.removeAddonListener(listener);
          throw new UnknownError(`Uninstall of ${id} has been canceled`);
        }
      },
      onUninstalled: addon => {
        if (addon.id === id) {
          AddonManager.removeAddonListener(listener);
          Promise.resolve();
        }
      },
    };

    AddonManager.addAddonListener(listener);
    addon.uninstall();
  });
};
