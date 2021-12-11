/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
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
ChromeUtils.defineModuleGetter(
  this,
  "NormandyUtils",
  "resource://normandy/lib/NormandyUtils.jsm"
);

var EXPORTED_SYMBOLS = ["PreferenceRolloutAction"];

const PREFERENCE_TYPE_MAP = {
  boolean: Services.prefs.PREF_BOOL,
  string: Services.prefs.PREF_STRING,
  number: Services.prefs.PREF_INT,
};

class PreferenceRolloutAction extends BaseAction {
  get schema() {
    return ActionSchemas["preference-rollout"];
  }

  async _run(recipe) {
    const args = recipe.arguments;

    // Check if the rollout is on the list of rollouts to stop applying.
    if (PreferenceRollouts.GRADUATION_SET.has(args.slug)) {
      this.log.debug(
        `Skipping rollout "${args.slug}" because it is in the graduation set.`
      );
      return;
    }

    // Determine which preferences are already being managed, to avoid
    // conflicts between recipes. This will throw if there is a problem.
    await this._verifyRolloutPrefs(args);

    const newRollout = {
      slug: args.slug,
      state: "active",
      preferences: args.preferences.map(({ preferenceName, value }) => ({
        preferenceName,
        value,
        previousValue: PrefUtils.getPref(preferenceName, { branch: "default" }),
      })),
    };

    const existingRollout = await PreferenceRollouts.get(args.slug);
    if (existingRollout) {
      const anyChanged = await this._updatePrefsForExistingRollout(
        existingRollout,
        newRollout
      );

      // If anything was different about the new rollout, write it to the db and send an event about it
      if (anyChanged) {
        await PreferenceRollouts.update(newRollout);
        TelemetryEvents.sendEvent("update", "preference_rollout", args.slug, {
          previousState: existingRollout.state,
          enrollmentId:
            existingRollout.enrollmentId ||
            TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
        });

        switch (existingRollout.state) {
          case PreferenceRollouts.STATE_ACTIVE: {
            this.log.debug(`Updated preference rollout ${args.slug}`);
            break;
          }
          case PreferenceRollouts.STATE_GRADUATED: {
            this.log.debug(`Ungraduated preference rollout ${args.slug}`);
            TelemetryEnvironment.setExperimentActive(
              args.slug,
              newRollout.state,
              { type: "normandy-prefrollout" }
            );
            break;
          }
          default: {
            Cu.reportError(
              new Error(
                `Updated pref rollout in unexpected state: ${existingRollout.state}`
              )
            );
          }
        }
      } else {
        this.log.debug(`No updates to preference rollout ${args.slug}`);
      }
    } else {
      // new enrollment
      // Check if this rollout would be a no-op, which is not allowed.
      if (
        newRollout.preferences.every(
          ({ value, previousValue }) => value === previousValue
        )
      ) {
        TelemetryEvents.sendEvent(
          "enrollFailed",
          "preference_rollout",
          args.slug,
          { reason: "would-be-no-op" }
        );
        // Throw so that this recipe execution is marked as a failure
        throw new Error(
          `New rollout ${args.slug} does not change any preferences.`
        );
      }

      let enrollmentId = NormandyUtils.generateUuid();
      newRollout.enrollmentId = enrollmentId;

      await PreferenceRollouts.add(newRollout);

      for (const { preferenceName, value } of args.preferences) {
        PrefUtils.setPref(preferenceName, value, { branch: "default" });
      }

      this.log.debug(`Enrolled in preference rollout ${args.slug}`);
      TelemetryEnvironment.setExperimentActive(args.slug, newRollout.state, {
        type: "normandy-prefrollout",
        enrollmentId: enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      });
      TelemetryEvents.sendEvent("enroll", "preference_rollout", args.slug, {
        enrollmentId: enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      });
    }
  }

  /**
   * Check that all the preferences in a rollout are ok to set. This means 1) no
   * other rollout is managing them, and 2) they match the types of the builtin
   * values.
   * @param {PreferenceRollout} rollout The arguments from a rollout recipe.
   * @throws If the preferences are not valid, with details in the error message.
   */
  async _verifyRolloutPrefs({ slug, preferences }) {
    const existingManagedPrefs = new Set();
    for (const rollout of await PreferenceRollouts.getAllActive()) {
      if (rollout.slug === slug) {
        continue;
      }
      for (const prefSpec of rollout.preferences) {
        existingManagedPrefs.add(prefSpec.preferenceName);
      }
    }

    for (const prefSpec of preferences) {
      if (existingManagedPrefs.has(prefSpec.preferenceName)) {
        TelemetryEvents.sendEvent("enrollFailed", "preference_rollout", slug, {
          reason: "conflict",
          preference: prefSpec.preferenceName,
        });
        // Throw so that this recipe execution is marked as a failure
        throw new Error(
          `Cannot start rollout ${slug}. Preference ${prefSpec.preferenceName} is already managed.`
        );
      }
      const existingPrefType = Services.prefs.getPrefType(
        prefSpec.preferenceName
      );
      const rolloutPrefType = PREFERENCE_TYPE_MAP[typeof prefSpec.value];

      if (
        existingPrefType !== Services.prefs.PREF_INVALID &&
        existingPrefType !== rolloutPrefType
      ) {
        TelemetryEvents.sendEvent("enrollFailed", "preference_rollout", slug, {
          reason: "invalid type",
          preference: prefSpec.preferenceName,
        });
        // Throw so that this recipe execution is marked as a failure
        throw new Error(
          `Cannot start rollout "${slug}" on "${prefSpec.preferenceName}". ` +
            `Existing preference is of type ${existingPrefType}, but rollout ` +
            `specifies type ${rolloutPrefType}`
        );
      }
    }
  }

  async _updatePrefsForExistingRollout(existingRollout, newRollout) {
    let anyChanged = false;
    const oldPrefSpecs = new Map(
      existingRollout.preferences.map(p => [p.preferenceName, p])
    );
    const newPrefSpecs = new Map(
      newRollout.preferences.map(p => [p.preferenceName, p])
    );

    // Check for any preferences that no longer exist, and un-set them.
    for (const { preferenceName, previousValue } of oldPrefSpecs.values()) {
      if (!newPrefSpecs.has(preferenceName)) {
        this.log.debug(
          `updating ${existingRollout.slug}: ${preferenceName} no longer exists`
        );
        anyChanged = true;
        PrefUtils.setPref(preferenceName, previousValue, { branch: "default" });
      }
    }

    // Check for any preferences that are new and need added, or changed and need updated.
    for (const prefSpec of Object.values(newRollout.preferences)) {
      let oldValue = null;
      if (oldPrefSpecs.has(prefSpec.preferenceName)) {
        let oldPrefSpec = oldPrefSpecs.get(prefSpec.preferenceName);
        oldValue = oldPrefSpec.value;

        // Trust the old rollout for the values of `previousValue`, but don't
        // consider this a change, since it already matches the DB, and doesn't
        // have any other stateful effect.
        prefSpec.previousValue = oldPrefSpec.previousValue;
      }
      if (oldValue !== newPrefSpecs.get(prefSpec.preferenceName).value) {
        anyChanged = true;
        this.log.debug(
          `updating ${existingRollout.slug}: ${prefSpec.preferenceName} value changed from ${oldValue} to ${prefSpec.value}`
        );
        PrefUtils.setPref(prefSpec.preferenceName, prefSpec.value, {
          branch: "default",
        });
      }
    }
    return anyChanged;
  }

  async _finalize() {
    await PreferenceRollouts.saveStartupPrefs();
  }
}
