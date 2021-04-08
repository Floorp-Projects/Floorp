/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BackgroundUpdate"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  TaskScheduler: "resource://gre/modules/TaskScheduler.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: "app.update.background.loglevel",
    prefix: "BackgroundUpdate",
  };
  return new ConsoleAPI(consoleOptions);
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

XPCOMUtils.defineLazyGetter(this, "localization", () => {
  return new Localization(
    ["branding/brand.ftl", "toolkit/updates/backgroundupdate.ftl"],
    true
  );
});

var BackgroundUpdate = {
  _initialized: false,

  get taskId() {
    let taskId = "backgroundupdate";
    if (AppConstants.platform == "win") {
      // In the future, we might lift this to TaskScheduler Win impl, so that
      // all tasks associated with this installation look consistent in the
      // Windows Task Scheduler UI.
      taskId = `${AppConstants.MOZ_APP_DISPLAYNAME_DO_NOT_USE} Background Update`;
    }
    return taskId;
  },

  /**
   * Whether this installation has an App and a GRE omnijar.
   *
   * Installations without an omnijar are generally developer builds and should not be updated.
   *
   * @returns {boolean} - true if this installation has an App and a GRE omnijar.
   */
  async _hasOmnijar() {
    const appOmniJar = FileUtils.getFile("XCurProcD", [
      AppConstants.OMNIJAR_NAME,
    ]);
    const greOmniJar = FileUtils.getFile("GreD", [AppConstants.OMNIJAR_NAME]);

    let bothExist =
      (await IOUtils.exists(appOmniJar.path)) &&
      (await IOUtils.exists(greOmniJar.path));

    return bothExist;
  },

  _force() {
    // We want to allow developers and testers to monkey with the system.
    return Services.prefs.getBoolPref("app.update.background.force", false);
  },

  _currentProfileIsDefaultProfile() {
    let defaultProfile;
    try {
      defaultProfile = ProfileService.defaultProfile;
    } catch (e) {}
    let currentProfile = ProfileService.currentProfile;
    // This comparison needs to accommodate null on both sides.
    let isDefaultProfile = defaultProfile && currentProfile == defaultProfile;
    return isDefaultProfile;
  },

  /**
   * Check eligibility criteria determining if this installation should be updated using the
   * background updater.
   *
   * These reasons should not factor in transient reasons, for example if there are currently multiple
   * Firefox instances running.
   *
   * Both the browser proper and the backgroundupdate background task invoke this function, so avoid
   * using profile specifics here.  Profile specifics that the background task specifically sources
   * from the default profile are available here.
   *
   * @returns [string] - descriptions of failed criteria; empty if all criteria were met.
   */
  async _reasonsToNotUpdateInstallation() {
    let SLUG = "_reasonsToNotUpdateInstallation";
    let reasons = [];

    log.debug(`${SLUG}: checking app.update.auto`);
    let updateAuto = await UpdateUtils.getAppUpdateAutoEnabled();
    if (!updateAuto) {
      reasons.push(this.REASON.NO_APP_UPDATE_AUTO);
    }

    log.debug(`${SLUG}: checking app.update.background.enabled`);
    let updateBackground = await UpdateUtils.readUpdateConfigSetting(
      "app.update.background.enabled"
    );
    if (!updateBackground) {
      reasons.push(this.REASON.NO_APP_UPDATE_BACKGROUND_ENABLED);
    }

    const bts =
      "@mozilla.org/backgroundtasks;1" in Cc &&
      Cc["@mozilla.org/backgroundtasks;1"].getService(Ci.nsIBackgroundTasks);

    log.debug(`${SLUG}: checking for MOZ_BACKGROUNDTASKS`);
    if (!AppConstants.MOZ_BACKGROUNDTASKS || !bts) {
      reasons.push(this.REASON.NO_MOZ_BACKGROUNDTASKS);
    }

    // The methods exposed by the update service named like `canX` answer the
    // question "can I do action X RIGHT NOW", where-as we want the variants
    // named like `canUsuallyX` to answer the question "can I usually do X, now
    // and in the future".
    let updateService = Cc["@mozilla.org/updates/update-service;1"].getService(
      Ci.nsIApplicationUpdateService
    );

    log.debug(
      `${SLUG}: checking that updates are not disabled by policy, testing ` +
        `configuration, or abnormal runtime environment`
    );
    if (!updateService.canUsuallyCheckForUpdates) {
      reasons.push(this.REASON.CANNOT_USUALLY_CHECK);
    }

    log.debug(
      `${SLUG}: checking that we can make progress: updates can stage and/or apply`
    );
    if (
      !updateService.canUsuallyStageUpdates &&
      !updateService.canUsuallyApplyUpdates
    ) {
      reasons.push(this.REASON.CANNOT_USUALLY_STAGE_AND_CANNOT_USUALLY_APPLY);
    }

    if (AppConstants.platform == "win") {
      if (!updateService.canUsuallyUseBits) {
        // There's no technical reason to require BITS, but the experience
        // without BITS will be worse because the background tasks will run
        // while downloading, consuming valuable resources.
        reasons.push(this.REASON.WINDOWS_CANNOT_USUALLY_USE_BITS);
      }
    }

    log.debug(`${SLUG}: checking that this installation has an omnijar`);
    if (!(await this._hasOmnijar())) {
      reasons.push(this.REASON.NO_OMNIJAR);
    }

    return reasons;
  },

  /**
   * Check if this particular profile should schedule tasks to update this installation using the
   * background updater.
   *
   * Only the browser proper should invoke this function, not background tasks, so this is the place
   * to use profile specifics.
   *
   * @returns [string] - descriptions of failed criteria; empty if all criteria were met.
   */
  async _reasonsToNotScheduleUpdates() {
    let SLUG = "_reasonsToNotScheduleUpdates";
    let reasons = [];

    const bts =
      "@mozilla.org/backgroundtasks;1" in Cc &&
      Cc["@mozilla.org/backgroundtasks;1"].getService(Ci.nsIBackgroundTasks);

    if (bts && bts.isBackgroundTaskMode) {
      throw new Components.Exception(
        `Not available in --backgroundtask mode`,
        Cr.NS_ERROR_NOT_AVAILABLE
      );
    }

    if (!this._currentProfileIsDefaultProfile()) {
      reasons.push(this.REASON.NOT_DEFAULT_PROFILE);
    }

    log.debug(`${SLUG}: checking app.update.langpack.enabled`);
    let updateLangpack = Services.prefs.getBoolPref(
      "app.update.langpack.enabled",
      true
    );
    if (updateLangpack) {
      log.debug(
        `${SLUG}: app.update.langpack.enabled=true, checking that no langpacks are installed`
      );

      let langpacks = await AddonManager.getAddonsByTypes(["locale"]);
      log.debug(`${langpacks.length} langpacks installed`);
      if (langpacks.length) {
        reasons.push(this.REASON.LANGPACK_INSTALLED);
      }
    }

    return reasons;
  },

  /**
   * Register a background update task.
   *
   * @param {string} [taskId]
   *        The task identifier; defaults to the platform-specific background update task ID.
   * @return {object} non-null if the background task was registered.
   */
  async _registerBackgroundUpdateTask(taskId = this.taskId) {
    let binary = Services.dirsvc.get("XREExeF", Ci.nsIFile);
    let args = [
      "--MOZ_LOG",
      // Note: `maxsize:1` means 1Mb total size, trimmed to 512kb on overflow.
      "sync,prependheader,timestamp,append,maxsize:1,Dump:5",
      "--MOZ_LOG_FILE",
      // The full path might hit command line length limits, but also makes it
      // much easier to find the relevant log file when starting from the
      // Windows Task Scheduler UI.
      FileUtils.getFile("UpdRootD", ["backgroundupdate.moz_log"]).path,
      "--backgroundtask",
      "backgroundupdate",
    ];

    let workingDirectory = FileUtils.getDir("UpdRootD", [], true).path;

    let description = await localization.formatValue(
      "backgroundupdate-task-description"
    );

    let result = await TaskScheduler.registerTask(
      taskId,
      binary.path,
      // Keep this default in sync with the preference in firefox.js.
      Services.prefs.getIntPref("app.update.background.interval", 60 * 60 * 7),
      {
        workingDirectory,
        args,
        description,
      }
    );

    return result;
  },

  async _mirrorToPerInstallationPref() {
    try {
      let scheduling = Services.prefs
        .getDefaultBranch("")
        .getBoolPref("app.update.background.scheduling.enabled");
      await UpdateUtils.writeUpdateConfigSetting(
        "app.update.background.enabled",
        scheduling,
        { setDefaultOnly: true }
      );
      log.debug(
        `mirrored per-profile pref "app.update.background.scheduling.enabled" default ` +
          `to per-installation pref default "app.update.background.enabled": ${scheduling}`
      );
    } catch (e) {
      if (e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_UNEXPECTED) {
        // The preference doesn't exist.
        return;
      }
      console.warn(
        `ignoring failure to mirror per-profile pref "app.update.background.scheduling.enabled" default ` +
          `to per-installation pref default "app.update.background.enabled"`,
        e
      );
    }
  },

  async observe(subject, topic, data) {
    let whatChanged;
    switch (topic) {
      case "nsPref:changed":
        whatChanged = `per-profile pref ${data}`;
        break;

      case "auto-update-config-change":
        whatChanged = `per-installation pref app.update.auto`;
        break;

      case "background-update-config-change":
        whatChanged = `per-installation pref app.update.background.enabled`;
        break;
    }

    log.debug(
      `observe: ${whatChanged} may have changed; invoking maybeScheduleBackgroundUpdateTask`
    );
    return this.maybeScheduleBackgroundUpdateTask();
  },

  /**
   * Maybe schedule (or unschedule) background tasks using OS-level task scheduling mechanisms.
   *
   * @return {boolean} true if a task is now scheduled, false otherwise.
   */
  async maybeScheduleBackgroundUpdateTask() {
    let SLUG = "maybeScheduleBackgroundUpdateTask";

    if (this._force() || this._currentProfileIsDefaultProfile()) {
      await this._mirrorToPerInstallationPref();
    }

    log.info(
      `${SLUG}: checking eligibility before scheduling background update task`
    );

    let previousEnabled;
    let successfullyReadPrevious;
    try {
      previousEnabled = await TaskScheduler.taskExists(this.taskId);
      successfullyReadPrevious = true;
    } catch (ex) {
      successfullyReadPrevious = false;
    }

    const previousReasons = Services.prefs.getCharPref(
      "app.update.background.previous.reasons",
      null
    );

    if (!this._initialized) {
      Services.obs.addObserver(this, "auto-update-config-change");
      Services.obs.addObserver(this, "background-update-config-change");

      // Witness when our own prefs change.
      Services.prefs.addObserver("app.update.background.force", this);
      Services.prefs.addObserver("app.update.background.interval", this);

      // To accommodate forcing with "app.update.background.force"
      // dynamically, we always observe
      // "app.update.background.scheduling.enabled", even though we act on it
      // (usually) only when we're the default profile.
      Services.prefs.addObserver(
        "app.update.background.scheduling.enabled",
        this
      );

      // Witness when the langpack updating feature is changed.
      Services.prefs.addObserver("app.update.langpack.enabled", this);

      // Witness when langpacks come and go.
      const onAddonEvent = async addon => {
        if (addon.type != "locale") {
          return;
        }
        log.debug(
          `${SLUG}: langpacks may have changed; invoking maybeScheduleBackgroundUpdateTask`
        );
        // No need to await this promise.
        this.maybeScheduleBackgroundUpdateTask();
      };
      const addonsListener = {
        onEnabled: onAddonEvent,
        onDisabled: onAddonEvent,
        onInstalled: onAddonEvent,
        onUninstalled: onAddonEvent,
      };
      AddonManager.addAddonListener(addonsListener);

      this._initialized = true;
    }

    log.debug(`${SLUG}: checking for reasons to not update this installation`);
    let reasons = await this._reasonsToNotUpdateInstallation();

    log.debug(
      `${SLUG}: checking for reasons to not schedule background updates with this profile`
    );
    let moreReasons = await this._reasonsToNotScheduleUpdates();
    reasons.push(...moreReasons);

    let enabled = !reasons.length;

    if (this._force()) {
      // We want to allow developers and testers to monkey with the system.
      log.debug(
        `${SLUG}: app.update.background.force=true, ignoring reasons: ${JSON.stringify(
          reasons
        )}`
      );
      reasons = [];
      enabled = true;
    }

    let updatePreviousPrefs = () => {
      if (reasons.length) {
        Services.prefs.setCharPref(
          "app.update.background.previous.reasons",
          JSON.stringify(reasons)
        );
      } else {
        Services.prefs.clearUserPref("app.update.background.previous.reasons");
      }
    };

    try {
      // Interacting with `TaskScheduler.jsm` can throw, so we'll catch.
      if (!enabled) {
        log.info(
          `${SLUG}: not scheduling background update: '${JSON.stringify(
            reasons
          )}'`
        );

        if (!successfullyReadPrevious || previousEnabled) {
          await TaskScheduler.deleteTask(this.taskId);
          log.debug(
            `${SLUG}: witnessed falling (enabled -> disabled) edge; deleted task ${this.taskId}.`
          );
        }

        updatePreviousPrefs();

        return false;
      }

      if (successfullyReadPrevious && previousEnabled) {
        log.info(
          `${SLUG}: background update was previously enabled; not registering task.`
        );

        return true;
      }

      log.info(
        `${SLUG}: background update was previously disabled for reasons: '${previousReasons}'`
      );

      await this._registerBackgroundUpdateTask(this.taskId);
      log.info(
        `${SLUG}: witnessed rising (disabled -> enabled) edge; registered task ${this.taskId}`
      );

      updatePreviousPrefs();

      return true;
    } catch (e) {
      log.error(
        `${SLUG}: exiting after uncaught exception in maybeScheduleBackgroundUpdateTask!`,
        e
      );

      return false;
    }
  },
};

BackgroundUpdate.REASON = {
  CANNOT_USUALLY_CHECK:
    "cannot usually check for updates due to policy, testing configuration, or runtime environment",
  CANNOT_USUALLY_STAGE_AND_CANNOT_USUALLY_APPLY:
    "updates cannot usually stage and cannot usually apply",
  LANGPACK_INSTALLED:
    "app.update.langpack.enabled=true and at least one langpack is installed",
  NOT_DEFAULT_PROFILE: "not default profile",
  NO_APP_UPDATE_AUTO: "app.update.auto=false",
  NO_APP_UPDATE_BACKGROUND_ENABLED: "app.update.background.enabled=false",
  NO_MOZ_BACKGROUNDTASKS: "MOZ_BACKGROUNDTASKS=0",
  NO_OMNIJAR: "no omnijar",
  WINDOWS_CANNOT_USUALLY_USE_BITS: "on Windows but cannot usually use BITS",
  OTHER_INSTANCE: "other instance is running",
};
