/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://normandy/actions/BaseAction.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEnvironment", "resource://gre/modules/TelemetryEnvironment.jsm");
ChromeUtils.defineModuleGetter(this, "PreferenceRollouts", "resource://normandy/lib/PreferenceRollouts.jsm");
ChromeUtils.defineModuleGetter(this, "PrefUtils", "resource://normandy/lib/PrefUtils.jsm");
ChromeUtils.defineModuleGetter(this, "ActionSchemas", "resource://normandy/actions/schemas/index.js");
ChromeUtils.defineModuleGetter(this, "TelemetryEvents", "resource://normandy/lib/TelemetryEvents.jsm");

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

    // First determine which preferences are already being managed, to avoid
    // conflicts between recipes. This will throw if there is a problem.
    await this._verifyRolloutPrefs(args);

    const newRollout = {
      slug: args.slug,
      state: "active",
      preferences: args.preferences.map(({preferenceName, value}) => ({
        preferenceName,
        value,
        previousValue: PrefUtils.getPref("default", preferenceName),
      })),
    };

    const existingRollout = await PreferenceRollouts.get(args.slug);
    if (existingRollout) {
      const anyChanged = await this._updatePrefsForExistingRollout(existingRollout, newRollout);

      // If anything was different about the new rollout, write it to the db and send an event about it
      if (anyChanged) {
        await PreferenceRollouts.update(newRollout);
        TelemetryEvents.sendEvent("update", "preference_rollout", args.slug, {previousState: existingRollout.state});

        switch (existingRollout.state) {
          case PreferenceRollouts.STATE_ACTIVE: {
            this.log.debug(`Updated preference rollout ${args.slug}`);
            break;
          }
          case PreferenceRollouts.STATE_GRADUATED: {
            this.log.debug(`Ungraduated preference rollout ${args.slug}`);
            TelemetryEnvironment.setExperimentActive(args.slug, newRollout.state, {type: "normandy-prefrollout"});
            break;
          }
          default: {
            Cu.reportError(new Error(`Updated pref rollout in unexpected state: ${existingRollout.state}`));
          }
        }
      } else {
        this.log.debug(`No updates to preference rollout ${args.slug}`);
      }

    } else { // new enrollment
      await PreferenceRollouts.add(newRollout);

      for (const {preferenceName, value} of args.preferences) {
        PrefUtils.setPref("default", preferenceName, value);
      }

      this.log.debug(`Enrolled in preference rollout ${args.slug}`);
      TelemetryEnvironment.setExperimentActive(args.slug, newRollout.state, {type: "normandy-prefrollout"});
      TelemetryEvents.sendEvent("enroll", "preference_rollout", args.slug, {});
    }
  }

  /**
   * Check that all the preferences in a rollout are ok to set. This means 1) no
   * other rollout is managing them, and 2) they match the types of the builtin
   * values.
   * @param {PreferenceRollout} rollout The arguments from a rollout recipe.
   * @throws If the preferences are not valid, with details in the error message.
   */
  async _verifyRolloutPrefs({slug, preferences}) {
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
        TelemetryEvents.sendEvent("enrollFailed", "preference_rollout", slug, {reason: "conflict", preference: prefSpec.preferenceName});
        throw new Error(`Cannot start rollout ${slug}. Preference ${prefSpec.preferenceName} is already managed.`);
      }
      const existingPrefType = Services.prefs.getPrefType(prefSpec.preferenceName);
      const rolloutPrefType = PREFERENCE_TYPE_MAP[typeof prefSpec.value];

      if (existingPrefType !== Services.prefs.PREF_INVALID && existingPrefType !== rolloutPrefType) {
        TelemetryEvents.sendEvent(
          "enrollFailed",
          "preference_rollout",
          slug,
          {reason: "invalid type", preference: prefSpec.preferenceName},
        );
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
    const oldPrefSpecs = new Map(existingRollout.preferences.map(p => [p.preferenceName, p]));
    const newPrefSpecs = new Map(newRollout.preferences.map(p => [p.preferenceName, p]));

    // Check for any preferences that no longer exist, and un-set them.
    for (const {preferenceName, previousValue} of oldPrefSpecs.values()) {
      if (!newPrefSpecs.has(preferenceName)) {
        this.log.debug(`updating ${existingRollout.slug}: ${preferenceName} no longer exists`);
        anyChanged = true;
        PrefUtils.setPref("default", preferenceName, previousValue);
      }
    }

    // Check for any preferences that are new and need added, or changed and need updated.
    for (const prefSpec of Object.values(newRollout.preferences)) {
      let oldValue = null;
      if (oldPrefSpecs.has(prefSpec.preferenceName)) {
        let oldPrefSpec = oldPrefSpecs.get(prefSpec.preferenceName);
        if (oldPrefSpec.previousValue !== prefSpec.previousValue) {
          this.log.debug(`updating ${existingRollout.slug}: ${prefSpec.preferenceName} previous value changed from ${oldPrefSpec.previousValue} to ${prefSpec.previousValue}`);
          prefSpec.previousValue = oldPrefSpec.previousValue;
          anyChanged = true;
        }
        oldValue = oldPrefSpec.value;
      }
      if (oldValue !== newPrefSpecs.get(prefSpec.preferenceName).value) {
        anyChanged = true;
        this.log.debug(`updating ${existingRollout.slug}: ${prefSpec.preferenceName} value changed from ${oldValue} to ${prefSpec.value}`);
        PrefUtils.setPref("default", prefSpec.preferenceName, prefSpec.value);
      }
    }
    return anyChanged;
  }

  async _finalize() {
    await PreferenceRollouts.saveStartupPrefs();
    await PreferenceRollouts.closeDB();
  }
}
