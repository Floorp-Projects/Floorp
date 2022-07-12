/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ActionSchemas: "resource://normandy/actions/schemas/index.js",
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonRollouts: "resource://normandy/lib/AddonRollouts.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.jsm",
});

var EXPORTED_SYMBOLS = ["AddonRollbackAction"];

class AddonRollbackAction extends BaseAction {
  get schema() {
    return lazy.ActionSchemas["addon-rollback"];
  }

  async _run(recipe) {
    const { rolloutSlug } = recipe.arguments;
    const rollout = await lazy.AddonRollouts.get(rolloutSlug);

    if (!rollout) {
      this.log.debug(`Rollback ${rolloutSlug} not applicable, skipping`);
      return;
    }

    switch (rollout.state) {
      case lazy.AddonRollouts.STATE_ACTIVE: {
        await lazy.AddonRollouts.update({
          ...rollout,
          state: lazy.AddonRollouts.STATE_ROLLED_BACK,
        });

        const addon = await lazy.AddonManager.getAddonByID(rollout.addonId);
        if (addon) {
          try {
            await addon.uninstall();
          } catch (err) {
            lazy.TelemetryEvents.sendEvent(
              "unenrollFailed",
              "addon_rollback",
              rolloutSlug,
              {
                reason: "uninstall-failed",
                enrollmentId:
                  rollout.enrollmentId ||
                  lazy.TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
              }
            );
            throw err;
          }
        } else {
          this.log.warn(
            `Could not uninstall addon ${rollout.addonId} for rollback ${rolloutSlug}: it is not installed.`
          );
        }

        lazy.TelemetryEvents.sendEvent(
          "unenroll",
          "addon_rollback",
          rolloutSlug,
          {
            reason: "rollback",
            enrollmentId:
              rollout.enrollmentId ||
              lazy.TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
          }
        );
        lazy.TelemetryEnvironment.setExperimentInactive(rolloutSlug);
        break;
      }

      case lazy.AddonRollouts.STATE_ROLLED_BACK: {
        return; // Do nothing
      }

      default: {
        throw new Error(
          `Unexpected state when rolling back ${rolloutSlug}: ${rollout.state}`
        );
      }
    }
  }
}
