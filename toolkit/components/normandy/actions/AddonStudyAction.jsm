/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This action handles the life cycle of add-on based studies. Currently that
 * means installing the add-on the first time the recipe applies to this client,
 * and uninstalling them when the recipe no longer applies.
 */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://normandy/actions/BaseAction.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  ActionSchemas: "resource://normandy/actions/schemas/index.js",
  AddonStudies: "resource://normandy/lib/AddonStudies.jsm",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.jsm",
});

var EXPORTED_SYMBOLS = ["AddonStudyAction"];

const OPT_OUT_STUDIES_ENABLED_PREF = "app.shield.optoutstudies.enabled";

class AddonStudyEnrollError extends Error {
  constructor(studyName, reason) {
    let message;
    switch (reason) {
      case "conflicting-addon-id": {
        message = "an add-on with this ID is already installed";
        break;
      }
      case "download-failure": {
        message = "the add-on failed to download";
        break;
      }
      default: {
        throw new Error(`Unexpected AddonStudyEnrollError reason: ${reason}`);
      }
    }
    super(new Error(`Cannot install study add-on for ${studyName}: ${message}.`));
    this.studyName = studyName;
    this.reason = reason;
  }
}

class AddonStudyAction extends BaseAction {
  get schema() {
    return ActionSchemas["addon-study"];
  }

  /**
   * This hook is executed once before any recipes have been processed, it is
   * responsible for:
   *
   *   - Checking if the user has opted out of studies, and if so, it disables the action.
   *   - Setting up tracking of seen recipes, for use in _finalize.
   */
  _preExecution() {
    // Check opt-out preference
    if (!Services.prefs.getBoolPref(OPT_OUT_STUDIES_ENABLED_PREF, true)) {
      this.log.info("User has opted-out of opt-out experiments, disabling action.");
      this.disable();
    }

    this.seenRecipeIds = new Set();
  }

  /**
   * This hook is executed once for each recipe that currently applies to this
   * client. It is responsible for:
   *
   *   - Enrolling studies the first time they are seen.
   *   - Marking studies as having been seen in this session.
   *
   * If the recipe fails to enroll, it should throw to properly report its status.
   */
  async _run(recipe) {
    this.seenRecipeIds.add(recipe.id);

    const hasStudy = await AddonStudies.has(recipe.id);
    if (recipe.arguments.isEnrollmentPaused || hasStudy) {
      // Recipe does not need anything done
      return;
    }

    await this.enroll(recipe);
  }

  /**
   * This hook is executed once after all recipes that apply to this client
   * have been processed. It is responsible for unenrolling the client from any
   * studies that no longer apply, based on this.seenRecipeIds.
   */
  async _finalize() {
    const activeStudies = (await AddonStudies.getAll()).filter(study => study.active);

    for (const study of activeStudies) {
      if (!this.seenRecipeIds.has(study.recipeId)) {
        this.log.debug(`Stopping study for recipe ${study.recipeId}`);
        try {
          await this.unenroll(study.recipeId, "recipe-not-seen");
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }
  }

  /**
   * Enroll in the study represented by the given recipe.
   * @param recipe Object describing the study to enroll in.
   */
  async enroll(recipe) {
    // This function first downloads the add-on to get its metadata. Then it
    // uses that metadata to record a study in `AddonStudies`. Then, it finishes
    // installing the add-on, and finally sends telemetry. If any of these steps
    // fails, the previous ones are undone, as needed.
    //
    // This ordering is important because the only intermediate states we can be
    // in are:
    //   1. The add-on is only downloaded, in which case AddonManager will clean it up.
    //   2. The study has been recorded, in which case we will unenroll on next
    //      start up, assuming that the add-on was uninstalled while the browser was
    //      shutdown.
    //   3. After installation is complete, but before telemetry, in which case we
    //      lose an enroll event. This is acceptable.
    //
    // This way we a shutdown, crash or unexpected error can't leave Normandy in
    // a long term inconsistent state. The main thing avoided is having a study
    // add-on installed but no record of it, which would leave it permanently
    // installed.

    const { addonUrl, name, description } = recipe.arguments;

    const downloadDeferred = PromiseUtils.defer();
    const installDeferred = PromiseUtils.defer();

    const install = await AddonManager.getInstallForURL(addonUrl, "application/x-xpinstall",
                                                        null, null, null, null, null,
                                                        {source: "internal"});

    const listener = {
      onDownloadFailed() {
        downloadDeferred.reject(new AddonStudyEnrollError(name, "download-failure"));
      },

      onDownloadEnded() {
        downloadDeferred.resolve();
        return false; // temporarily pause installation for Normandy bookkeeping
      },

      onInstallStarted(cbInstall) {
        if (cbInstall.existingAddon) {
          installDeferred.reject(new AddonStudyEnrollError(name, "conflicting-addon-id"));
          return false; // cancel the installation, no upgrades allowed
        }
        return true;
      },

      onInstallFailed() {
        installDeferred.reject(new AddonStudyEnrollError(name, "failed-to-install"));
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
      this.reportEnrollError(err);
      install.removeListener(listener);
      return;
    }

    const addonId = install.addon.id;

    const study = {
      recipeId: recipe.id,
      name,
      description,
      addonId,
      addonVersion: install.addon.version,
      addonUrl,
      active: true,
      studyStartDate: new Date(),
    };

    try {
      await AddonStudies.add(study);
    } catch (err) {
      this.reportEnrollError(err);
      install.removeListener(listener);
      install.cancel();
      throw err;
    }

    // finish paused installation
    try {
      install.install();
      await installDeferred.promise;
    } catch (err) {
      this.reportEnrollError(err);
      install.removeListener(listener);
      await AddonStudies.delete(recipe.id);
      throw err;
    }

    // All done, report success to Telemetry and cleanup
    TelemetryEvents.sendEvent("enroll", "addon_study", name, {
      addonId: install.addon.id,
      addonVersion: install.addon.version,
    });

    install.removeListener(listener);
  }

  reportEnrollError(error) {
    if (error instanceof AddonStudyEnrollError) {
      // One of our known errors. Report it nicely to telemetry
      TelemetryEvents.sendEvent("enrollFailed", "addon_study", error.studyName, { reason: error.reason });
    } else {
      /*
        * Some unknown error. Add some helpful details, and report it to
        * telemetry. The actual stack trace and error message could possibly
        * contain PII, so we don't include them here. Instead include some
        * information that should still be helpful, and is less likely to be
        * unsafe.
        */
      const safeErrorMessage = `${error.fileName}:${error.lineNumber}:${error.columnNumber} ${error.name}`;
      TelemetryEvents.sendEvent("enrollFailed", "addon_study", error.studyName, {
        reason: safeErrorMessage.slice(0, 80),  // max length is 80 chars
      });
    }
  }

  /**
   * Unenrolls the client from the study with a given recipe ID.
   * @param recipeId The recipe ID of an enrolled study
   * @param reason The reason for this unenrollment, to be used in Telemetry
   * @throws If the specified study does not exist, or if it is already inactive.
   */
  async unenroll(recipeId, reason = "unknown") {
    const study = await AddonStudies.get(recipeId);
    if (!study) {
      throw new Error(`No study found for recipe ${recipeId}.`);
    }
    if (!study.active) {
      throw new Error(`Cannot stop study for recipe ${recipeId}; it is already inactive.`);
    }

    await AddonStudies.markAsEnded(study, reason);

    const addon = await AddonManager.getAddonByID(study.addonId);
    if (addon) {
      await addon.uninstall();
    } else {
      this.log.warn(`Could not uninstall addon ${study.addonId} for recipe ${study.recipeId}: it is not installed.`);
    }
  }
}
