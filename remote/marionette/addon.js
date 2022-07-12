/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Addon"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",

  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
});

// from https://developer.mozilla.org/en-US/Add-ons/Add-on_Manager/AddonManager#AddonInstall_errors
const ERRORS = {
  [-1]: "ERROR_NETWORK_FAILURE: A network error occured.",
  [-2]: "ERROR_INCORECT_HASH: The downloaded file did not match the expected hash.",
  [-3]: "ERROR_CORRUPT_FILE: The file appears to be corrupt.",
  [-4]: "ERROR_FILE_ACCESS: There was an error accessing the filesystem.",
  [-5]: "ERROR_SIGNEDSTATE_REQUIRED: The addon must be signed and isn't.",
};

async function installAddon(file) {
  let install = await lazy.AddonManager.getInstallForFile(file, null, {
    source: "internal",
  });

  if (install.error) {
    throw new lazy.error.UnknownError(ERRORS[install.error]);
  }

  return install.install().catch(err => {
    throw new lazy.error.UnknownError(ERRORS[install.error]);
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
      file = new lazy.FileUtils.File(path);
    } catch (e) {
      throw new lazy.error.UnknownError(`Expected absolute path: ${e}`, e);
    }

    if (!file.exists()) {
      throw new lazy.error.UnknownError(`No such file or directory: ${path}`);
    }

    try {
      if (temporary) {
        addon = await lazy.AddonManager.installTemporaryAddon(file);
      } else {
        addon = await installAddon(file);
      }
    } catch (e) {
      throw new lazy.error.UnknownError(
        `Could not install add-on: ${path}: ${e.message}`,
        e
      );
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
    let candidate = await lazy.AddonManager.getAddonByID(id);
    if (candidate === null) {
      // `AddonManager.getAddonByID` never rejects but instead
      // returns `null` if the requested addon cannot be found.
      throw new lazy.error.UnknownError(`Addon ${id} is not installed`);
    }

    return new Promise(resolve => {
      let listener = {
        onOperationCancelled: addon => {
          if (addon.id === candidate.id) {
            lazy.AddonManager.removeAddonListener(listener);
            throw new lazy.error.UnknownError(
              `Uninstall of ${candidate.id} has been canceled`
            );
          }
        },

        onUninstalled: addon => {
          if (addon.id === candidate.id) {
            lazy.AddonManager.removeAddonListener(listener);
            resolve();
          }
        },
      };

      lazy.AddonManager.addAddonListener(listener);
      candidate.uninstall();
    });
  }
}
