/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BackgroundUpdate"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { BackgroundTasksManager } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  BackgroundTasksUtils: "resource://gre/modules/BackgroundTasksUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  TaskScheduler: "resource://gre/modules/TaskScheduler.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
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

XPCOMUtils.defineLazyGetter(lazy, "localization", () => {
  return new Localization(
    ["branding/brand.ftl", "toolkit/updates/backgroundupdate.ftl"],
    true
  );
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "UpdateService",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);

// We may want to change the definition of the task over time. When we do this,
// we need to remove and re-register the task. We will make sure this happens
// by storing the installed version number of the task to a pref and comparing
// that version number to the current version. If they aren't equal, we know
// that we have to re-register the task.
const TASK_DEF_CURRENT_VERSION = 3;
const TASK_INSTALLED_VERSION_PREF =
  "app.update.background.lastInstalledTaskVersion";

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
    const appOmniJar = lazy.FileUtils.getFile("XCurProcD", [
      AppConstants.OMNIJAR_NAME,
    ]);
    const greOmniJar = lazy.FileUtils.getFile("GreD", [
      AppConstants.OMNIJAR_NAME,
    ]);

    let bothExist =
      (await IOUtils.exists(appOmniJar.path)) &&
      (await IOUtils.exists(greOmniJar.path));

    return bothExist;
  },

  _force() {
    // We want to allow developers and testers to monkey with the system.
    return Services.prefs.getBoolPref("app.update.background.force", false);
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

    lazy.log.debug(`${SLUG}: checking app.update.auto`);
    let updateAuto = await lazy.UpdateUtils.getAppUpdateAutoEnabled();
    if (!updateAuto) {
      reasons.push(this.REASON.NO_APP_UPDATE_AUTO);
    }

    lazy.log.debug(`${SLUG}: checking app.update.background.enabled`);
    let updateBackground = await lazy.UpdateUtils.readUpdateConfigSetting(
      "app.update.background.enabled"
    );
    if (!updateBackground) {
      reasons.push(this.REASON.NO_APP_UPDATE_BACKGROUND_ENABLED);
    }

    const bts =
      "@mozilla.org/backgroundtasks;1" in Cc &&
      Cc["@mozilla.org/backgroundtasks;1"].getService(Ci.nsIBackgroundTasks);

    lazy.log.debug(`${SLUG}: checking for MOZ_BACKGROUNDTASKS`);
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

    lazy.log.debug(
      `${SLUG}: checking that updates are not disabled by policy, testing ` +
        `configuration, or abnormal runtime environment`
    );
    if (!updateService.canUsuallyCheckForUpdates) {
      reasons.push(this.REASON.CANNOT_USUALLY_CHECK);
    }

    lazy.log.debug(
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

    lazy.log.debug(`${SLUG}: checking that this installation has an omnijar`);
    if (!(await this._hasOmnijar())) {
      reasons.push(this.REASON.NO_OMNIJAR);
    }

    if (updateService.manualUpdateOnly) {
      reasons.push(this.REASON.MANUAL_UPDATE_ONLY);
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

    // No default profile happens under xpcshell but also when running local
    // builds.  It's unexpected in the wild so we track it separately.
    if (!lazy.BackgroundTasksUtils.hasDefaultProfile()) {
      reasons.push(this.REASON.NO_DEFAULT_PROFILE_EXISTS);
    }

    if (!lazy.BackgroundTasksUtils.currentProfileIsDefaultProfile()) {
      reasons.push(this.REASON.NOT_DEFAULT_PROFILE);
    }

    lazy.log.debug(`${SLUG}: checking app.update.langpack.enabled`);
    let updateLangpack = Services.prefs.getBoolPref(
      "app.update.langpack.enabled",
      true
    );
    if (updateLangpack) {
      lazy.log.debug(
        `${SLUG}: app.update.langpack.enabled=true, checking that no langpacks are installed`
      );

      let langpacks = await lazy.AddonManager.getAddonsByTypes(["locale"]);
      lazy.log.debug(`${langpacks.length} langpacks installed`);
      if (langpacks.length) {
        reasons.push(this.REASON.LANGPACK_INSTALLED);
      }
    }

    let serviceRegKeyExists;
    try {
      serviceRegKeyExists = Cc["@mozilla.org/updates/update-processor;1"]
        .createInstance(Ci.nsIUpdateProcessor)
        .getServiceRegKeyExists();
    } catch (ex) {
      lazy.log.error(
        `${SLUG}: Failed to check for Maintenance Service Registry Key: ${ex}`
      );
      serviceRegKeyExists = false;
    }
    if (!serviceRegKeyExists) {
      reasons.push(this.REASON.SERVICE_REGISTRY_KEY_MISSING);
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
      lazy.FileUtils.getFile("UpdRootD", ["backgroundupdate.moz_log"]).path,
      "--backgroundtask",
      "backgroundupdate",
    ];

    let workingDirectory = lazy.FileUtils.getDir("UpdRootD", [], true).path;

    let description = await lazy.localization.formatValue(
      "backgroundupdate-task-description"
    );

    // Let the task run for a maximum of 20 minutes before the task scheduler
    // stops it.
    let executionTimeoutSec = 20 * 60;

    let result = await lazy.TaskScheduler.registerTask(
      taskId,
      binary.path,
      // Keep this default in sync with the preference in firefox.js.
      Services.prefs.getIntPref("app.update.background.interval", 60 * 60 * 7),
      {
        workingDirectory,
        args,
        description,
        executionTimeoutSec,
      }
    );

    Services.prefs.setIntPref(
      TASK_INSTALLED_VERSION_PREF,
      TASK_DEF_CURRENT_VERSION
    );

    return result;
  },

  /**
   * Background Update is controlled by the per-installation pref
   * "app.update.background.enabled". When Background Update was still in the
   * experimental phase, the default value of this pref may have been changed.
   * Now that the feature has been rolled out, we need to make sure that the
   * desired default value is restored.
   */
  async ensureExperimentToRolloutTransitionPerformed() {
    if (!lazy.UpdateUtils.PER_INSTALLATION_PREFS_SUPPORTED) {
      return;
    }
    const transitionPerformedPref = "app.update.background.rolledout";
    if (Services.prefs.getBoolPref(transitionPerformedPref, false)) {
      // writeUpdateConfigSetting serializes access to the config file. Because
      // of this, we can safely return here without worrying about another call
      // to this function that might still be in progress.
      return;
    }
    Services.prefs.setBoolPref(transitionPerformedPref, true);

    const defaultValue =
      lazy.UpdateUtils.PER_INSTALLATION_PREFS["app.update.background.enabled"]
        .defaultValue;
    await lazy.UpdateUtils.writeUpdateConfigSetting(
      "app.update.background.enabled",
      defaultValue,
      { setDefaultOnly: true }
    );

    // To be thorough, remove any traces of the pref that used to control the
    // default value that we just set. We don't want any users to have the
    // impression that that pref is still useful.
    Services.prefs.clearUserPref("app.update.background.scheduling.enabled");
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

    lazy.log.debug(
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

    await this.ensureExperimentToRolloutTransitionPerformed();

    lazy.log.info(
      `${SLUG}: checking eligibility before scheduling background update task`
    );

    let previousEnabled;
    let successfullyReadPrevious;
    try {
      previousEnabled = await lazy.TaskScheduler.taskExists(this.taskId);
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

      // Witness when the langpack updating feature is changed.
      Services.prefs.addObserver("app.update.langpack.enabled", this);

      // Witness when langpacks come and go.
      const onAddonEvent = async addon => {
        if (addon.type != "locale") {
          return;
        }
        lazy.log.debug(
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
      lazy.AddonManager.addAddonListener(addonsListener);

      this._initialized = true;
    }

    lazy.log.debug(
      `${SLUG}: checking for reasons to not update this installation`
    );
    let reasons = await this._reasonsToNotUpdateInstallation();

    lazy.log.debug(
      `${SLUG}: checking for reasons to not schedule background updates with this profile`
    );
    let moreReasons = await this._reasonsToNotScheduleUpdates();
    reasons.push(...moreReasons);

    let enabled = !reasons.length;

    if (this._force()) {
      // We want to allow developers and testers to monkey with the system.
      lazy.log.debug(
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
        lazy.log.info(
          `${SLUG}: not scheduling background update: '${JSON.stringify(
            reasons
          )}'`
        );

        if (!successfullyReadPrevious || previousEnabled) {
          await lazy.TaskScheduler.deleteTask(this.taskId);
          lazy.log.debug(
            `${SLUG}: witnessed falling (enabled -> disabled) edge; deleted task ${this.taskId}.`
          );
        }

        updatePreviousPrefs();

        return false;
      }

      if (successfullyReadPrevious && previousEnabled) {
        let taskInstalledVersion = Services.prefs.getIntPref(
          TASK_INSTALLED_VERSION_PREF,
          1
        );
        if (taskInstalledVersion == TASK_DEF_CURRENT_VERSION) {
          lazy.log.info(
            `${SLUG}: background update was previously enabled; not registering task.`
          );

          return true;
        }
        lazy.log.info(
          `${SLUG}: Detected task version change from ` +
            `${taskInstalledVersion} to ${TASK_DEF_CURRENT_VERSION}. ` +
            `Removing task so the new version can be registered`
        );
        try {
          await lazy.TaskScheduler.deleteTask(this.taskId);
        } catch (e) {
          lazy.log.error(`${SLUG}: Error removing old task: ${e}`);
        }
        try {
          // When the update directory was moved, we migrated the old contents
          // to the new location. This can potentially happen in a background
          // task. However, we also need to re-register the background task
          // with the task scheduler in order to update the MOZ_LOG_FILE value
          // to point to the new location. If the task runs before Firefox has
          // a chance to re-register the task, the log file may be recreated in
          // the old location. In practice, this would be unusual, because
          // MOZ_LOG_FILE will not create the parent directories necessary to
          // put a log file in the specified location. But just to be safe,
          // we'll do some cleanup when we re-register the task to make sure
          // that no log file is hanging around in the old location.
          let oldUpdateDir = lazy.FileUtils.getDir("OldUpdRootD", [], false);
          let oldLog = oldUpdateDir.clone();
          oldLog.append("backgroundupdate.moz_log");

          if (oldLog.exists()) {
            oldLog.remove(false);
            // We may have created some directories in order to put this log
            // file in this location. Clean them up if they are empty.
            // (If we pass false for the recurse parameter, directories with
            // contents will not be removed)
            //
            // Potentially removes "C:\ProgramData\Mozilla\updates\<hash>"
            oldUpdateDir.remove(false);
            // Potentially removes "C:\ProgramData\Mozilla\updates"
            oldUpdateDir.parent.remove(false);
            // Potentially removes "C:\ProgramData\Mozilla"
            oldUpdateDir.parent.parent.remove(false);
          }
        } catch (ex) {
          lazy.log.warn(
            `${SLUG}: Ignoring error encountered attempting to remove stale log file: ${ex}`
          );
        }
      }

      lazy.log.info(
        `${SLUG}: background update was previously disabled for reasons: '${previousReasons}'`
      );

      await this._registerBackgroundUpdateTask(this.taskId);
      lazy.log.info(
        `${SLUG}: witnessed rising (disabled -> enabled) edge; registered task ${this.taskId}`
      );

      updatePreviousPrefs();

      return true;
    } catch (e) {
      lazy.log.error(
        `${SLUG}: exiting after uncaught exception in maybeScheduleBackgroundUpdateTask!`,
        e
      );

      return false;
    }
  },

  /**
   * Record parts of the update environment for our custom Glean ping.
   *
   * This is just like the Telemetry Environment, but pared down to what we're
   * likely to use in background update-specific analyses.
   *
   * Right now this is only for use in the background update task, but after Bug
   * 1703313 (migrating AUS telemetry to be Glean-aware) we might use it more
   * generally.
   */
  async recordUpdateEnvironment() {
    try {
      Glean.update.serviceEnabled.set(
        Services.prefs.getBoolPref("app.update.service.enabled", false)
      );
    } catch (e) {
      // It's fine if some or all of these are missing.
    }

    // In the background update task, this should always be enabled, but let's
    // find out if there's an error in the system.
    Glean.update.autoDownload.set(
      await lazy.UpdateUtils.getAppUpdateAutoEnabled()
    );
    Glean.update.backgroundUpdate.set(
      await lazy.UpdateUtils.readUpdateConfigSetting(
        "app.update.background.enabled"
      )
    );

    Glean.update.channel.set(lazy.UpdateUtils.UpdateChannel);
    Glean.update.enabled.set(
      !Services.policies || Services.policies.isAllowed("appUpdate")
    );

    Glean.update.canUsuallyApplyUpdates.set(
      lazy.UpdateService.canUsuallyApplyUpdates
    );
    Glean.update.canUsuallyCheckForUpdates.set(
      lazy.UpdateService.canUsuallyCheckForUpdates
    );
    Glean.update.canUsuallyStageUpdates.set(
      lazy.UpdateService.canUsuallyStageUpdates
    );
    Glean.update.canUsuallyUseBits.set(lazy.UpdateService.canUsuallyUseBits);
  },
};

BackgroundUpdate.REASON = {
  CANNOT_USUALLY_CHECK:
    "cannot usually check for updates due to policy, testing configuration, or runtime environment",
  CANNOT_USUALLY_STAGE_AND_CANNOT_USUALLY_APPLY:
    "updates cannot usually stage and cannot usually apply",
  LANGPACK_INSTALLED:
    "app.update.langpack.enabled=true and at least one langpack is installed",
  MANUAL_UPDATE_ONLY: "the ManualAppUpdateOnly policy is enabled",
  NO_DEFAULT_PROFILE_EXISTS: "no default profile exists",
  NOT_DEFAULT_PROFILE: "not default profile",
  NO_APP_UPDATE_AUTO: "app.update.auto=false",
  NO_APP_UPDATE_BACKGROUND_ENABLED: "app.update.background.enabled=false",
  NO_MOZ_BACKGROUNDTASKS: "MOZ_BACKGROUNDTASKS=0",
  NO_OMNIJAR: "no omnijar",
  SERVICE_REGISTRY_KEY_MISSING:
    "the maintenance service registry key is not present",
  WINDOWS_CANNOT_USUALLY_USE_BITS: "on Windows but cannot usually use BITS",
};

/**
 * Specific exit codes for `--backgroundtask backgroundupdate`.
 *
 * These help distinguish common failure cases.  In particular, they distinguish
 * "default profile does not exist" from "default profile cannot be locked" from
 * more general errors reading from the default profile.
 */
BackgroundUpdate.EXIT_CODE = {
  ...BackgroundTasksManager.EXIT_CODE,
  // We clone the other exit codes simply so we can use one object for all the codes.
  DEFAULT_PROFILE_DOES_NOT_EXIST: 11,
  DEFAULT_PROFILE_CANNOT_BE_LOCKED: 12,
  DEFAULT_PROFILE_CANNOT_BE_READ: 13,
  // Another instance is running.
  OTHER_INSTANCE: 21,
};
