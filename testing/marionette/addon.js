/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
ChromeUtils.import("resource://gre/modules/FileUtils.jsm");

const {UnknownError} = ChromeUtils.import("chrome://marionette/content/error.js", {});

this.EXPORTED_SYMBOLS = ["Addon"];

// from https://developer.mozilla.org/en-US/Add-ons/Add-on_Manager/AddonManager#AddonInstall_errors
const ERRORS = {
  [-1]: "ERROR_NETWORK_FAILURE: A network error occured.",
  [-2]: "ERROR_INCORECT_HASH: The downloaded file did not match the expected hash.",
  [-3]: "ERROR_CORRUPT_FILE: The file appears to be corrupt.",
  [-4]: "ERROR_FILE_ACCESS: There was an error accessing the filesystem.",
  [-5]: "ERROR_SIGNEDSTATE_REQUIRED: The addon must be signed and isn't.",
};

async function installAddon(file) {
  let install = await AddonManager.getInstallForFile(file);

  return new Promise(resolve => {
    if (install.error) {
      throw new UnknownError(ERRORS[install.error]);
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
        throw new UnknownError(ERRORS[install.error]);
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

/** Installs addons by path and uninstalls by ID. */
class Addon {
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
  static async install(path, temporary = false) {
    let addon;
    let file;

    try {
      file = new FileUtils.File(path);
    } catch (e) {
      throw new UnknownError(`Expected absolute path: ${e}`, e);
    }

    if (!file.exists()) {
      throw new UnknownError(`No such file or directory: ${path}`);
    }

    try {
      if (temporary) {
        addon = await AddonManager.installTemporaryAddon(file);
      } else {
        addon = await installAddon(file);
      }
    } catch (e) {
      throw new UnknownError(
          `Could not install add-on: ${path}: ${e.message}`, e);
    }

    return addon.id;
  }

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
  static async uninstall(id) {
    let candidate = await AddonManager.getAddonByID(id);

    return new Promise(resolve => {
      let listener = {
        onOperationCancelled: addon => {
          if (addon.id === candidate.id) {
            AddonManager.removeAddonListener(listener);
            throw new UnknownError(`Uninstall of ${candidate.id} has been canceled`);
          }
        },

        onUninstalled: addon => {
          if (addon.id === candidate.id) {
            AddonManager.removeAddonListener(listener);
            resolve();
          }
        },
      };

      AddonManager.addAddonListener(listener);
      candidate.uninstall();
    });
  }
}
this.Addon = Addon;
