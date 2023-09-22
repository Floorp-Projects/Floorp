/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { EXIT_CODE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  BackgroundTasksUtils: "resource://gre/modules/BackgroundTasksUtils.sys.mjs",
  JSONFile: "resource://gre/modules/JSONFile.sys.mjs",
  TaskScheduler: "resource://gre/modules/TaskScheduler.sys.mjs",
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ASRouterTargeting: "resource://activity-stream/lib/ASRouterTargeting.jsm",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.sys.mjs for details.
    maxLogLevel: "error",
    maxLogLevelPref: "app.update.background.loglevel",
    prefix: "BackgroundUpdate",
  };
  return new ConsoleAPI(consoleOptions);
});

ChromeUtils.defineLazyGetter(lazy, "localization", () => {
  return new Localization(
    ["branding/brand.ftl", "toolkit/updates/backgroundupdate.ftl"],
    true
  );
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  idleService: ["@mozilla.org/widget/useridleservice;1", "nsIUserIdleService"],
  UpdateService: [
    "@mozilla.org/updates/update-service;1",
    "nsIApplicationUpdateService",
  ],
});

// We may want to change the definition of the task over time. When we do this,
// we need to remove and re-register the task. We will make sure this happens
// by storing the installed version number of the task to a pref and comparing
// that version number to the current version. If they aren't equal, we know
// that we have to re-register the task.
const TASK_DEF_CURRENT_VERSION = 3;
const TASK_INSTALLED_VERSION_PREF =
  "app.update.background.lastInstalledTaskVersion";

export var BackgroundUpdate = {
  QueryInterface: ChromeUtils.generateQI([
    "nsINamed",
    "nsIObserver",
    "nsITimerCallback",
  ]),
  name: "BackgroundUpdate",

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
    const appOmniJar = PathUtils.join(
      Services.dirsvc.get("XCurProcD", Ci.nsIFile).path,
      AppConstants.OMNIJAR_NAME
    );
    const greOmniJar = PathUtils.join(
      Services.dirsvc.get("GreD", Ci.nsIFile).path,
      AppConstants.OMNIJAR_NAME
    );

    let bothExist =
      (await IOUtils.exists(appOmniJar)) && (await IOUtils.exists(greOmniJar));

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

    lazy.log.debug(
      `${SLUG}: checking that we are on a supported OS (currently only Windows)`
    );
    if (AppConstants.platform != "win") {
      reasons.push(this.REASON.UNSUPPORTED_OS);
    }

    if (AppConstants.platform == "win") {
      lazy.log.debug(`${SLUG}: checking that we can usually use Windows BITS`);
      if (!updateService.canUsuallyUseBits) {
        // There's no technical reason to require BITS, but the experience
        // without BITS will be worse because the background tasks will run
        // while downloading, consuming valuable resources.
        reasons.push(this.REASON.WINDOWS_CANNOT_USUALLY_USE_BITS);
      }

      // Historically the background update process assumed the Mozilla
      // Maintenance Service was available and could update this installation.
      // We want to handle unelevated installations where this is not the case,
      // and for flexibility we are rolling this out behind a Nimbus feature.
      lazy.log.debug(
        `${SLUG}: checking that the Mozilla Maintenance Service Registry key exists, ` +
          `or that the unelevated installs are permitted`
      );
      let serviceRegKeyExists = false;
      try {
        serviceRegKeyExists = Cc["@mozilla.org/updates/update-processor;1"]
          .createInstance(Ci.nsIUpdateProcessor)
          .getServiceRegKeyExists();
      } catch (ex) {
        lazy.log.error(
          `${SLUG}: Failed to check for Maintenance Service Registry Key: ${ex}`
        );
      }

      if (!serviceRegKeyExists) {
        // A Nimbus rollout sets this preference and allows users with
        // unelevated installations to update in the background. For that to
        // work we use the setPref function to toggle a preference, because the
        // value for Nimbus is currently not readable in a backgroundtask. The
        // preference serves in that case as our communication channel.
        let allowUnelevated = await Services.prefs.getBoolPref(
          "app.update.background.allowUpdatesForUnelevatedInstallations"
        );

        if (!allowUnelevated) {
          // With the nimbus feature disabled and without the registry key we
          // do not want to attempt an update for unelevated installations.
          reasons.push(this.REASON.SERVICE_REGISTRY_KEY_MISSING);
        } else {
          // We record in telemetry, that the service registry key is missing
          // and the experiment is enabled. This is the first time that the
          // Nimbus feature could impact Firefox behaviour.
          lazy.NimbusFeatures.backgroundUpdate.recordExposureEvent();
          lazy.log.debug(
            `${SLUG}: ` +
              "expermiment active: trying to update unelevated installations."
          );

          // Now check if the path is writable. If not we are dealing with an
          // elevated installation and cannot update it without the service for
          // which the registry key is missing at this point.
          if (!updateService.isAppBaseDirWritable) {
            reasons.push(this.REASON.SERVICE_REGISTRY_KEY_MISSING);
            reasons.push(this.REASON.APPBASEDIR_NOT_WRITABLE);
          }
        }
      }
    }

    lazy.log.debug(`${SLUG}: checking that this installation has an omnijar`);
    if (!(await this._hasOmnijar())) {
      reasons.push(this.REASON.NO_OMNIJAR);
    }

    if (updateService.manualUpdateOnly) {
      reasons.push(this.REASON.MANUAL_UPDATE_ONLY);
    }

    this._recordGleanMetrics(reasons);

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

    this._recordGleanMetrics(reasons);

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
      PathUtils.join(
        Services.dirsvc.get("UpdRootD", Ci.nsIFile).path,
        "backgroundupdate.moz_log"
      ),
      "--backgroundtask",
      "backgroundupdate",
    ];

    let workingDirectory = Services.dirsvc.get("UpdRootD", Ci.nsIFile).path;
    await IOUtils.makeDirectory(workingDirectory, { ignoreExisting: true });

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

  observe(subject, topic, data) {
    let whatChanged;
    switch (topic) {
      case "idle-daily":
        this._snapshot.saveSoon();
        return;

      case "user-interaction-active":
        this._startTargetingSnapshottingTimer();
        Services.obs.removeObserver(this, "idle-daily");
        Services.obs.removeObserver(this, "user-interaction-active");
        lazy.log.debug(
          `observe: ${topic}; started targeting snapshotting timer`
        );
        return;

      case "nsPref:changed":
        whatChanged = `per-profile pref ${data}`;
        break;

      case "auto-update-config-change":
        whatChanged = `per-installation pref app.update.auto`;
        break;

      case "background-update-config-change":
        whatChanged = `per-installation pref app.update.background.enabled`;
        break;

      case "nimbus-update":
        whatChanged = `Nimbus ${data}`;
        break;
    }

    lazy.log.debug(
      `observe: ${whatChanged} may have changed; invoking maybeScheduleBackgroundUpdateTask`
    );
    this.maybeScheduleBackgroundUpdateTask();
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

    // datetime with an empty parameter records 'now'
    Glean.backgroundUpdate.timeLastUpdateScheduled.set();

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
      lazy.NimbusFeatures.backgroundUpdate.onUpdate((event, reason) => {
        this.observe(null, "nimbus-update", reason);
      });

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
          let oldUpdateDir = Services.dirsvc.get(
            "OldUpdRootD",
            Ci.nsIFile
          ).path;
          let oldLog = PathUtils.join(oldUpdateDir, "backgroundupdate.moz_log");

          if (await IOUtils.exists(oldLog)) {
            try {
              await IOUtils.remove(oldLog);
              // We may have created some directories in order to put this log
              // file in this location. Clean them up if they are empty.
              //
              // Potentially removes "C:\ProgramData\Mozilla\updates\<hash>"
              await IOUtils.remove(oldUpdateDir);
              // Potentially removes "C:\ProgramData\Mozilla\updates"
              await IOUtils.remove(PathUtils.parent(oldUpdateDir));
              // Potentially removes "C:\ProgramData\Mozilla"
              await IOUtils.remove(PathUtils.parent(oldUpdateDir, 2));
            } catch (e) {
              if (
                !(
                  DOMException.isInstance(e) &&
                  e.name === "OperationError" &&
                  e.message.includes(
                    "Could not remove the non-empty directory at"
                  )
                )
              ) {
                throw e;
              }
            }
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

  /**
   * Schedule periodic snapshotting of the Firefox Messaging System
   * targeting configuration.
   *
   * The background update task will target messages based on the
   * latest snapshot of the default profile's targeting configuration.
   */
  async scheduleFirefoxMessagingSystemTargetingSnapshotting() {
    let SLUG = "scheduleFirefoxMessagingSystemTargetingSnapshotting";
    let path = PathUtils.join(PathUtils.profileDir, "targeting.snapshot.json");

    let snapshot = new lazy.JSONFile({
      beforeSave: async () => {
        if (Services.startup.shuttingDown) {
          // Collecting targeting information can be slow and cause shutdown
          // crashes.  Just write what we have in that case.  During shutdown,
          // the regular log apparatus is not available, so use `dump`.
          if (lazy.log.shouldLog("debug")) {
            dump(
              `${SLUG}: shutting down, so not updating Firefox Messaging System targeting information from beforeSave\n`
            );
          }
          return;
        }

        lazy.log.debug(
          `${SLUG}: preparing to write Firefox Messaging System targeting information to ${path}`
        );

        // Merge latest data into existing data.  This data may be partial, due
        // to runtime errors and abbreviated collections, especially when
        // shutting down.  We accept the risk of incomplete or even internally
        // inconsistent data: it's generally better to have stale data (and
        // potentially target a user as they appeared in the past) than to block
        // shutdown for more accurate results.  An alternate approach would be
        // to restrict the targeting data collected, but it's hard to
        // distinguish expensive collection operations and the system loses
        // flexibility when restrictions of this type are added.
        let latestData = await lazy.ASRouterTargeting.getEnvironmentSnapshot();
        // We expect to always have data, but: belt-and-braces.
        if (snapshot?.data?.environment) {
          Object.assign(snapshot.data.environment, latestData.environment);
        } else {
          snapshot.data = latestData;
        }
      },
      path,
    });

    // We don't `load`, since we don't care about reading existing (now stale)
    // data.
    snapshot.data = await lazy.ASRouterTargeting.getEnvironmentSnapshot();

    // Persist.
    snapshot.saveSoon();

    this._snapshot = snapshot;

    // Continue persisting periodically.  `JSONFile.sys.mjs` will also persist one
    // last time before shutdown.
    // Hold a reference to prevent GC.
    this._targetingSnapshottingTimer = Cc[
      "@mozilla.org/timer;1"
    ].createInstance(Ci.nsITimer);
    // By default, snapshot Firefox Messaging System targeting for use by the
    // background update task every 60 minutes.
    this._targetingSnapshottingTimerIntervalSec = Services.prefs.getIntPref(
      "app.update.background.messaging.targeting.snapshot.intervalSec",
      3600
    );
    this._startTargetingSnapshottingTimer();
  },

  // nsITimerCallback
  notify() {
    const SLUG = "_targetingSnapshottingTimerCallback";

    if (Services.startup.shuttingDown) {
      // Collecting targeting information can be slow and cause shutdown
      // crashes, so if we're shutting down, don't try to collect.  During
      // shutdown, the regular log apparatus is not available, so use `dump`.
      if (lazy.log.shouldLog("debug")) {
        dump(
          `${SLUG}: shutting down, so not updating Firefox Messaging System targeting information from timer\n`
        );
      }
      return;
    }

    this._snapshot.saveSoon();

    if (
      lazy.idleService.idleTime >
      this._targetingSnapshottingTimerIntervalSec * 1000
    ) {
      lazy.log.debug(
        `${SLUG}: idle time longer than interval, adding observers`
      );
      Services.obs.addObserver(this, "idle-daily");
      Services.obs.addObserver(this, "user-interaction-active");
    } else {
      lazy.log.debug(`${SLUG}: restarting timer`);
      this._startTargetingSnapshottingTimer();
    }
  },

  _startTargetingSnapshottingTimer() {
    this._targetingSnapshottingTimer.initWithCallback(
      this,
      this._targetingSnapshottingTimerIntervalSec * 1000,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  /**
   * Reads the snapshotted Firefox Messaging System targeting out of a profile.
   * Collects background update specific telemetry.  Never throws.
   *
   * If no `lock` is given, the default profile is locked and the preferences
   * read from it.  If `lock` is given, read from the given lock's directory.
   *
   * @param {nsIProfileLock} [lock] optional lock to use
   * @returns {object} possibly empty targeting snapshot.
   */
  async readFirefoxMessagingSystemTargetingSnapshot(lock = null) {
    let SLUG = "readFirefoxMessagingSystemTargetingSnapshot";

    let defaultProfileTargetingSnapshot = {};

    Glean.backgroundUpdate.targetingExists.set(false);
    Glean.backgroundUpdate.targetingException.set(true);
    try {
      defaultProfileTargetingSnapshot =
        await lazy.BackgroundTasksUtils.readFirefoxMessagingSystemTargetingSnapshot(
          lock
        );
      Glean.backgroundUpdate.targetingExists.set(true);
      Glean.backgroundUpdate.targetingException.set(false);

      if (defaultProfileTargetingSnapshot?.version) {
        Glean.backgroundUpdate.targetingVersion.set(
          defaultProfileTargetingSnapshot.version
        );
      }
      if (defaultProfileTargetingSnapshot?.environment?.firefoxVersion) {
        Glean.backgroundUpdate.targetingEnvFirefoxVersion.set(
          defaultProfileTargetingSnapshot.environment.firefoxVersion
        );
      }
      if (defaultProfileTargetingSnapshot?.environment?.currentDate) {
        Glean.backgroundUpdate.targetingEnvCurrentDate.set(
          // Glean date times are provided in nanoseconds, `getTime()` yields
          // milliseconds (after the Unix epoch).
          new Date(
            defaultProfileTargetingSnapshot.environment.currentDate
          ).getTime() * 1000
        );
      }
      if (defaultProfileTargetingSnapshot?.environment?.profileAgeCreated) {
        Glean.backgroundUpdate.targetingEnvProfileAge.set(
          // Glean date times are provided in nanoseconds, `profileAgeCreated`
          // is in milliseconds (after the Unix epoch).
          defaultProfileTargetingSnapshot.environment.profileAgeCreated * 1000
        );
      }
    } catch (f) {
      if (DOMException.isInstance(f) && f.name === "NotFoundError") {
        Glean.backgroundUpdate.targetingException.set(false);
        lazy.log.info(`${SLUG}: no default profile targeting snapshot exists`);
      } else {
        lazy.log.warn(
          `${SLUG}: ignoring exception reading default profile targeting snapshot`,
          f
        );
      }
    }

    return defaultProfileTargetingSnapshot;
  },

  /**
   * Local helper function to record all reasons why the background updater is
   * not used with Glean. This function will only track the first 20 reasons.
   * It is also fault tolerant and will only display debug messages if the
   * metric cannot be recorded for any reason.
   *
   * @param {array of strings} [reasons]
   *        a list of BackgroundUpdate.REASON values (=> string)
   */
  async _recordGleanMetrics(reasons) {
    // Record Glean metrics with all the reasons why the update was impossible.
    for (const [key, value] of Object.entries(this.REASON)) {
      if (reasons.includes(value)) {
        try {
          // `testGetValue` throws a `DataError` in case
          // of `InvalidOverflow` and other outstanding errors.
          Glean.backgroundUpdate.reasonsToNotUpdate.testGetValue();
          Glean.backgroundUpdate.reasonsToNotUpdate.add(key);
        } catch (e) {
          // Debug print an error message and break the loop to avoid Glean
          // messages on the console would otherwise be caused by the add().
          lazy.log.debug("Error recording reasonsToNotUpdate");
          console.log("Error recording reasonsToNotUpdate");
          break;
        }
      }
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
  MANUAL_UPDATE_ONLY: "the ManualAppUpdateOnly policy is enabled",
  NO_DEFAULT_PROFILE_EXISTS: "no default profile exists",
  NOT_DEFAULT_PROFILE: "not default profile",
  NO_APP_UPDATE_AUTO: "app.update.auto=false",
  NO_APP_UPDATE_BACKGROUND_ENABLED: "app.update.background.enabled=false",
  NO_MOZ_BACKGROUNDTASKS: "MOZ_BACKGROUNDTASKS=0",
  NO_OMNIJAR: "no omnijar",
  SERVICE_REGISTRY_KEY_MISSING:
    "the maintenance service registry key is not present",
  UNSUPPORTED_OS: "unsupported OS",
  WINDOWS_CANNOT_USUALLY_USE_BITS: "on Windows but cannot usually use BITS",
  APPBASEDIR_NOT_WRITABLE: "the base directory is not writable",
};

/**
 * Specific exit codes for `--backgroundtask backgroundupdate`.
 *
 * These help distinguish common failure cases.  In particular, they distinguish
 * "default profile does not exist" from "default profile cannot be locked" from
 * more general errors reading from the default profile.
 */
BackgroundUpdate.EXIT_CODE = {
  ...EXIT_CODE,
  // We clone the other exit codes simply so we can use one object for all the codes.
  DEFAULT_PROFILE_DOES_NOT_EXIST: 11,
  DEFAULT_PROFILE_CANNOT_BE_LOCKED: 12,
  DEFAULT_PROFILE_CANNOT_BE_READ: 13,
  // Another instance is running.
  OTHER_INSTANCE: 21,
};
