/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseAction } from "resource://normandy/actions/BaseAction.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ActionSchemas: "resource://normandy/actions/schemas/index.sys.mjs",
  AddonRollouts: "resource://normandy/lib/AddonRollouts.sys.mjs",
  NormandyAddonManager: "resource://normandy/lib/NormandyAddonManager.sys.mjs",
  NormandyApi: "resource://normandy/lib/NormandyApi.sys.mjs",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.sys.mjs",
});

class AddonRolloutError extends Error {
  /**
   * @param {string} slug
   * @param {object} extra Extra details to include when reporting the error to telemetry.
   * @param {string} extra.reason The specific reason for the failure.
   */
  constructor(slug, extra) {
    let message;
    let { reason } = extra;
    switch (reason) {
      case "conflict": {
        message = "an existing rollout already exists for this add-on";
        break;
      }
      case "addon-id-changed": {
        message = "upgrade add-on ID does not match installed add-on ID";
        break;
      }
      case "upgrade-required": {
        message = "a newer version of the add-on is already installed";
        break;
      }
      case "download-failure": {
        message = "the add-on failed to download";
        break;
      }
      case "metadata-mismatch": {
        message = "the server metadata does not match the downloaded add-on";
        break;
      }
      case "install-failure": {
        message = "the add-on failed to install";
        break;
      }
      default: {
        throw new Error(`Unexpected AddonRolloutError reason: ${reason}`);
      }
    }
    super(`Cannot install add-on for rollout (${slug}): ${message}.`);
    this.slug = slug;
    this.extra = extra;
  }
}

export class AddonRolloutAction extends BaseAction {
  get schema() {
    return lazy.ActionSchemas["addon-rollout"];
  }

  async _run(recipe) {
    const { extensionApiId, slug } = recipe.arguments;

    const existingRollout = await lazy.AddonRollouts.get(slug);
    const eventName = existingRollout ? "update" : "enroll";
    const extensionDetails = await lazy.NormandyApi.fetchExtensionDetails(
      extensionApiId
    );

    // Check if the existing rollout matches the current rollout
    if (
      existingRollout &&
      existingRollout.addonId === extensionDetails.extension_id
    ) {
      const versionCompare = Services.vc.compare(
        existingRollout.addonVersion,
        extensionDetails.version
      );

      if (versionCompare === 0) {
        return; // Do nothing
      }
    }

    const createError = (reason, extra = {}) => {
      return new AddonRolloutError(slug, {
        ...extra,
        reason,
      });
    };

    // Check for a conflict (addon already installed by another rollout)
    const activeRollouts = await lazy.AddonRollouts.getAllActive();
    const conflictingRollout = activeRollouts.find(
      rollout =>
        rollout.slug !== slug &&
        rollout.addonId === extensionDetails.extension_id
    );
    if (conflictingRollout) {
      const conflictError = createError("conflict", {
        addonId: conflictingRollout.addonId,
        conflictingSlug: conflictingRollout.slug,
      });
      this.reportError(conflictError, "enrollFailed");
      throw conflictError;
    }

    const onInstallStarted = (install, installDeferred) => {
      const existingAddon = install.existingAddon;

      if (existingRollout && existingRollout.addonId !== install.addon.id) {
        installDeferred.reject(createError("addon-id-changed"));
        return false; // cancel the upgrade, the add-on ID has changed
      }

      if (
        existingAddon &&
        Services.vc.compare(existingAddon.version, install.addon.version) > 0
      ) {
        installDeferred.reject(createError("upgrade-required"));
        return false; // cancel the installation, must be an upgrade
      }

      return true;
    };

    const applyNormandyChanges = async install => {
      const details = {
        addonId: install.addon.id,
        addonVersion: install.addon.version,
        extensionApiId,
        xpiUrl: extensionDetails.xpi,
        xpiHash: extensionDetails.hash,
        xpiHashAlgorithm: extensionDetails.hash_algorithm,
      };

      if (existingRollout) {
        await lazy.AddonRollouts.update({
          ...existingRollout,
          ...details,
        });
      } else {
        await lazy.AddonRollouts.add({
          recipeId: recipe.id,
          state: lazy.AddonRollouts.STATE_ACTIVE,
          slug,
          ...details,
        });
      }
    };

    const undoNormandyChanges = async () => {
      if (existingRollout) {
        await lazy.AddonRollouts.update(existingRollout);
      } else {
        await lazy.AddonRollouts.delete(recipe.id);
      }
    };

    const [installedId, installedVersion] =
      await lazy.NormandyAddonManager.downloadAndInstall({
        createError,
        extensionDetails,
        applyNormandyChanges,
        undoNormandyChanges,
        onInstallStarted,
        reportError: error => this.reportError(error, `${eventName}Failed`),
      });

    if (existingRollout) {
      this.log.debug(`Updated addon rollout ${slug}`);
    } else {
      this.log.debug(`Enrolled in addon rollout ${slug}`);
      lazy.TelemetryEnvironment.setExperimentActive(
        slug,
        lazy.AddonRollouts.STATE_ACTIVE,
        {
          type: "normandy-addonrollout",
        }
      );
    }

    // All done, report success to Telemetry
    lazy.TelemetryEvents.sendEvent(eventName, "addon_rollout", slug, {
      addonId: installedId,
      addonVersion: installedVersion,
    });
  }

  reportError(error, eventName) {
    if (error instanceof AddonRolloutError) {
      // One of our known errors. Report it nicely to telemetry
      lazy.TelemetryEvents.sendEvent(
        eventName,
        "addon_rollout",
        error.slug,
        error.extra
      );
    } else {
      /*
       * Some unknown error. Add some helpful details, and report it to
       * telemetry. The actual stack trace and error message could possibly
       * contain PII, so we don't include them here. Instead include some
       * information that should still be helpful, and is less likely to be
       * unsafe.
       */
      const safeErrorMessage = `${error.fileName}:${error.lineNumber}:${error.columnNumber} ${error.name}`;
      lazy.TelemetryEvents.sendEvent(eventName, "addon_rollout", error.slug, {
        reason: safeErrorMessage.slice(0, 80), // max length is 80 chars
      });
    }
  }
}
