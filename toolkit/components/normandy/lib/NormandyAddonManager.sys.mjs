/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

export const NormandyAddonManager = {
  async downloadAndInstall({
    createError,
    extensionDetails,
    applyNormandyChanges,
    undoNormandyChanges,
    onInstallStarted,
    reportError,
  }) {
    const { extension_id, hash, hash_algorithm, version, xpi } =
      extensionDetails;

    const downloadDeferred = Promise.withResolvers();
    const installDeferred = Promise.withResolvers();

    const install = await lazy.AddonManager.getInstallForURL(xpi, {
      hash: `${hash_algorithm}:${hash}`,
      telemetryInfo: { source: "internal" },
    });

    const listener = {
      onInstallStarted(cbInstall) {
        const versionMatches = cbInstall.addon.version === version;
        const idMatches = cbInstall.addon.id === extension_id;

        if (!versionMatches || !idMatches) {
          installDeferred.reject(createError("metadata-mismatch"));
          return false; // cancel the installation, server metadata does not match downloaded add-on
        }

        if (onInstallStarted) {
          return onInstallStarted(cbInstall, installDeferred);
        }

        return true;
      },

      onDownloadFailed() {
        downloadDeferred.reject(
          createError("download-failure", {
            detail: lazy.AddonManager.errorToString(install.error),
          })
        );
      },

      onDownloadEnded() {
        downloadDeferred.resolve();
        return false; // temporarily pause installation for Normandy bookkeeping
      },

      onInstallFailed() {
        installDeferred.reject(
          createError("install-failure", {
            detail: lazy.AddonManager.errorToString(install.error),
          })
        );
      },

      onInstallEnded() {
        installDeferred.resolve();
      },
    };

    install.addListener(listener);

    // Download the add-on
    try {
      install.install();
      await downloadDeferred.promise;
    } catch (err) {
      reportError(err);
      install.removeListener(listener);
      throw err;
    }

    // Complete any book-keeping
    try {
      await applyNormandyChanges(install);
    } catch (err) {
      reportError(err);
      install.removeListener(listener);
      install.cancel();
      throw err;
    }

    // Finish paused installation
    try {
      install.install();
      await installDeferred.promise;
    } catch (err) {
      reportError(err);
      install.removeListener(listener);
      await undoNormandyChanges();
      throw err;
    }

    install.removeListener(listener);

    return [install.addon.id, install.addon.version];
  },
};
