/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PreferenceRollouts",
  "resource://normandy/lib/PreferenceRollouts.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrefUtils",
  "resource://normandy/lib/PrefUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ActionSchemas",
  "resource://normandy/actions/schemas/index.js"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEvents",
  "resource://normandy/lib/TelemetryEvents.jsm"
);

var EXPORTED_SYMBOLS = ["PreferenceRollbackAction"];

class PreferenceRollbackAction extends BaseAction {
  get schema() {
    return ActionSchemas["preference-rollback"];
  }

  async _run(recipe) {
    const { rolloutSlug } = recipe.arguments;
    const rollout = await PreferenceRollouts.get(rolloutSlug);

    if (PreferenceRollouts.GRADUATION_SET.has(rolloutSlug)) {
      // graduated rollouts can't be rolled back
      TelemetryEvents.sendEvent(
        "unenrollFailed",
        "preference_rollback",
        rolloutSlug,
        {
          reason: "in-graduation-set",
          enrollmentId:
            rollout?.enrollmentId ?? TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
        }
      );
      throw new Error(
        `Cannot rollback rollout in graduation set "${rolloutSlug}".`
      );
    }

    if (!rollout) {
      this.log.debug(`Rollback ${rolloutSlug} not applicable, skipping`);
      return;
    }

    switch (rollout.state) {
      case PreferenceRollouts.STATE_ACTIVE: {
        this.log.info(`Rolling back ${rolloutSlug}`);
        rollout.state = PreferenceRollouts.STATE_ROLLED_BACK;
        for (const { preferenceName, previousValue } of rollout.preferences) {
          PrefUtils.setPref(preferenceName, previousValue, {
            branch: "default",
          });
        }
        await PreferenceRollouts.update(rollout);
        TelemetryEvents.sendEvent(
          "unenroll",
          "preference_rollback",
          rolloutSlug,
          {
            reason: "rollback",
            enrollmentId:
              rollout.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
          }
        );
        TelemetryEnvironment.setExperimentInactive(rolloutSlug);
        break;
      }
      case PreferenceRollouts.STATE_ROLLED_BACK: {
        // The rollout has already been rolled back, so nothing to do here.
        break;
      }
      case PreferenceRollouts.STATE_GRADUATED: {
        // graduated rollouts can't be rolled back
        TelemetryEvents.sendEvent(
          "unenrollFailed",
          "preference_rollback",
          rolloutSlug,
          {
            reason: "graduated",
            enrollmentId:
              rollout.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
          }
        );
        throw new Error(
          `Cannot rollback already graduated rollout ${rolloutSlug}`
        );
      }
      default: {
        throw new Error(
          `Unexpected state when rolling back ${rolloutSlug}: ${rollout.state}`
        );
      }
    }
  }

  async _finalize() {
    await PreferenceRollouts.saveStartupPrefs();
  }
}
