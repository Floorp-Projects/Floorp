/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { AddonStudies } = ChromeUtils.import(
  "resource://normandy/lib/AddonStudies.jsm"
);
const { PreferenceExperiments } = ChromeUtils.import(
  "resource://normandy/lib/PreferenceExperiments.jsm"
);
const { RecipeRunner } = ChromeUtils.import(
  "resource://normandy/lib/RecipeRunner.jsm"
);

var EXPORTED_SYMBOLS = ["NormandyMigrations"];

const BOOTSTRAP_LOGGER_NAME = "app.normandy.bootstrap";

const PREF_PREFIX = "app.normandy";
const LEGACY_PREF_PREFIX = "extensions.shield-recipe-client";
const PREF_LOGGING_LEVEL = "app.normandy.logging.level";
const PREF_MIGRATIONS_APPLIED = "app.normandy.migrationsApplied";
const PREF_OPTOUTSTUDIES_ENABLED = "app.shield.optoutstudies.enabled";

// Logging
const log = Log.repository.getLogger(BOOTSTRAP_LOGGER_NAME);
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
log.level = Services.prefs.getIntPref(PREF_LOGGING_LEVEL, Log.Level.Warn);

const NormandyMigrations = {
  async applyAll() {
    let migrationsApplied = Services.prefs.getIntPref(
      PREF_MIGRATIONS_APPLIED,
      0
    );

    for (let i = migrationsApplied; i < this.migrations.length; i++) {
      await this.applyOne(i);
      migrationsApplied++;
      Services.prefs.setIntPref(PREF_MIGRATIONS_APPLIED, migrationsApplied);
    }
  },

  async applyOne(id) {
    const migration = this.migrations[id];
    log.debug(`Running Normandy migration ${migration.name}`);
    await migration();
  },

  migrations: [
    migrateShieldPrefs,
    migrateStudiesEnabledWithoutHealthReporting,
    AddonStudies.migrations
      .migration01AddonStudyFieldsToSlugAndUserFacingFields,
    PreferenceExperiments.migrations.migration01MoveExperiments,
    PreferenceExperiments.migrations.migration02MultiPreference,
    PreferenceExperiments.migrations.migration03AddActionName,
    PreferenceExperiments.migrations.migration04RenameNameToSlug,
    RecipeRunner.migrations.migration01RemoveOldRecipesCollection,
    AddonStudies.migrations.migration02RemoveOldAddonStudyAction,
    migrateRemoveLastBuildIdPref,
    PreferenceExperiments.migrations.migration05RemoveOldAction,
    PreferenceExperiments.migrations.migration06TrackOverriddenPrefs,
  ],
};

function migrateShieldPrefs() {
  const legacyBranch = Services.prefs.getBranch(LEGACY_PREF_PREFIX + ".");
  const newBranch = Services.prefs.getBranch(PREF_PREFIX + ".");

  for (const prefName of legacyBranch.getChildList("")) {
    const legacyPrefType = legacyBranch.getPrefType(prefName);
    const newPrefType = newBranch.getPrefType(prefName);

    // If new preference exists and is not the same as the legacy pref, skip it
    if (
      newPrefType !== Services.prefs.PREF_INVALID &&
      newPrefType !== legacyPrefType
    ) {
      log.error(
        `Error migrating normandy pref ${prefName}; pref type does not match.`
      );
      continue;
    }

    // Now move the value over. If it matches the default, this will be a no-op
    switch (legacyPrefType) {
      case Services.prefs.PREF_STRING:
        newBranch.setCharPref(prefName, legacyBranch.getCharPref(prefName));
        break;

      case Services.prefs.PREF_INT:
        newBranch.setIntPref(prefName, legacyBranch.getIntPref(prefName));
        break;

      case Services.prefs.PREF_BOOL:
        newBranch.setBoolPref(prefName, legacyBranch.getBoolPref(prefName));
        break;

      case Services.prefs.PREF_INVALID:
        // This should never happen.
        log.error(
          `Error migrating pref ${prefName}; pref type is invalid (${legacyPrefType}).`
        );
        break;

      default:
        // This should never happen either.
        log.error(
          `Error getting startup pref ${prefName}; unknown value type ${legacyPrefType}.`
        );
    }

    legacyBranch.clearUserPref(prefName);
  }
}

/**
 * Migration to handle moving the studies opt-out pref from under the health
 * report upload pref to an independent pref.
 *
 * If the pref was set to true and the health report upload pref was set
 * to true then the pref should stay true. Otherwise set it to false.
 */
function migrateStudiesEnabledWithoutHealthReporting() {
  const optOutStudiesEnabled = Services.prefs.getBoolPref(
    PREF_OPTOUTSTUDIES_ENABLED,
    false
  );
  const healthReportUploadEnabled = Services.prefs.getBoolPref(
    "datareporting.healthreport.uploadEnabled",
    false
  );
  Services.prefs.setBoolPref(
    PREF_OPTOUTSTUDIES_ENABLED,
    optOutStudiesEnabled && healthReportUploadEnabled
  );
}

/**
 * Tracking last build ID is now done by comparing Services.appinfo.appBuildID
 * and Services.appinfo.lastAppBuildID. Remove the manual tracking.
 */
function migrateRemoveLastBuildIdPref() {
  Services.prefs.clearUserPref("app.normandy.last_seen_buildid");
}
