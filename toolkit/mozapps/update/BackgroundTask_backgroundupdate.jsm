/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "backgroundTaskTimeoutSec",
  "maybeSubmitBackgroundUpdatePing",
  "runBackgroundTask",
];

const { BackgroundUpdate } = ChromeUtils.import(
  "resource://gre/modules/BackgroundUpdate.jsm"
);
const { EXIT_CODE } = BackgroundUpdate;
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  BackgroundTasksUtils: "resource://gre/modules/BackgroundTasksUtils.jsm",
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "UpdateService",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);

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

const backgroundTaskTimeoutSec = Services.prefs.getIntPref(
  "app.update.background.timeoutSec",
  10 * 60
);

/**
 * Verify that pre-conditions to update this installation (both persistent and
 * transient) are fulfilled, and if they are all fulfilled, pump the update
 * loop.
 *
 * This means checking for, downloading, and potentially applying updates.
 *
 * @returns {boolean} - `true` if an update loop was completed.
 */
async function _attemptBackgroundUpdate() {
  let SLUG = "_attemptBackgroundUpdate";

  // Here's where we do `post-update-processing`.  Creating the stub invokes the
  // `UpdateServiceStub()` constructor, which handles various migrations (which should not be
  // necessary, but we want to run for consistency and any migrations added in the future) and then
  // dispatches `post-update-processing` (if appropriate).  We want to do this very early, so that
  // the real update service is in its fully initialized state before any usage.
  lazy.log.debug(
    `${SLUG}: creating UpdateServiceStub() for "post-update-processing"`
  );
  Cc["@mozilla.org/updates/update-service-stub;1"].createInstance(
    Ci.nsISupports
  );

  lazy.log.debug(
    `${SLUG}: checking for preconditions necessary to update this installation`
  );
  let reasons = await BackgroundUpdate._reasonsToNotUpdateInstallation();

  if (BackgroundUpdate._force()) {
    // We want to allow developers and testers to monkey with the system.
    lazy.log.debug(
      `${SLUG}: app.update.background.force=true, ignoring reasons: ${JSON.stringify(
        reasons
      )}`
    );
    reasons = [];
  }

  reasons.sort();
  for (let reason of reasons) {
    Glean.backgroundUpdate.reasons.add(reason);
  }

  let enabled = !reasons.length;
  if (!enabled) {
    lazy.log.info(
      `${SLUG}: not running background update task: '${JSON.stringify(
        reasons
      )}'`
    );

    return false;
  }

  let result = new Promise(resolve => {
    let appUpdater = new lazy.AppUpdater();

    let _appUpdaterListener = (status, progress, progressMax) => {
      let stringStatus = lazy.AppUpdater.STATUS.debugStringFor(status);
      Glean.backgroundUpdate.states.add(stringStatus);
      Glean.backgroundUpdate.finalState.set(stringStatus);

      if (lazy.AppUpdater.STATUS.isTerminalStatus(status)) {
        lazy.log.debug(
          `${SLUG}: background update transitioned to terminal status ${status}: ${stringStatus}`
        );
        appUpdater.removeListener(_appUpdaterListener);
        resolve(true);
      } else if (status == lazy.AppUpdater.STATUS.CHECKING) {
        // The usual initial flow for the Background Update Task is to kick off
        // the update download and immediately exit. For consistency, we are
        // going to enforce this flow. So if we are just now checking for
        // updates, we will limit the updater such that it cannot start staging,
        // even if we immediately download the entire update.
        lazy.log.debug(
          `${SLUG}: This session will be limited to downloading updates only.`
        );
        lazy.UpdateService.onlyDownloadUpdatesThisSession = true;
      } else if (
        status == lazy.AppUpdater.STATUS.DOWNLOADING &&
        (lazy.UpdateService.onlyDownloadUpdatesThisSession ||
          (progress !== undefined && progressMax !== undefined))
      ) {
        // We get a DOWNLOADING callback with no progress or progressMax values
        // when we initially switch to the DOWNLOADING state. But when we get
        // onProgress notifications, progress and progressMax will be defined.
        // Remember to keep in mind that progressMax is a required value that
        // we can count on being meaningful, but it will be set to -1 for BITS
        // transfers that haven't begun yet.
        if (
          lazy.UpdateService.onlyDownloadUpdatesThisSession ||
          progressMax < 0 ||
          progress != progressMax
        ) {
          lazy.log.debug(
            `${SLUG}: Download in progress. Exiting task while download ` +
              `transfers`
          );
          // If the download is still in progress, we don't want the Background
          // Update Task to hang around waiting for it to complete.
          lazy.UpdateService.onlyDownloadUpdatesThisSession = true;

          appUpdater.removeListener(_appUpdaterListener);
          resolve(true);
        } else {
          lazy.log.debug(`${SLUG}: Download has completed!`);
        }
      } else {
        lazy.log.debug(
          `${SLUG}: background update transitioned to status ${status}: ${stringStatus}`
        );
      }
    };
    appUpdater.addListener(_appUpdaterListener);

    appUpdater.check();
  });

  return result;
}

/**
 * Maybe submit a "background-update" custom Glean ping.
 *
 * If data reporting upload in general is enabled Glean will submit a ping.  To determine if
 * telemetry is enabled, Glean will look at the relevant pref, which was mirrored from the default
 * profile.  Note that the Firefox policy mechanism will manage this pref, locking it to particular
 * values as appropriate.
 */
async function maybeSubmitBackgroundUpdatePing() {
  let SLUG = "maybeSubmitBackgroundUpdatePing";

  // It should be possible to turn AUSTLMY data into Glean data, but mapping histograms isn't
  // trivial, so we don't do it at this time.  Bug 1703313.

  GleanPings.backgroundUpdate.submit();

  lazy.log.info(`${SLUG}: submitted "background-update" ping`);
}

async function runBackgroundTask(commandLine) {
  let SLUG = "runBackgroundTask";
  lazy.log.error(`${SLUG}: backgroundupdate`);

  // Help debugging.  This is a pared down version of
  // `dataProviders.application` in `Troubleshoot.jsm`.  When adding to this
  // debugging data, try to follow the form from that module.
  let data = {
    name: Services.appinfo.name,
    osVersion:
      Services.sysinfo.getProperty("name") +
      " " +
      Services.sysinfo.getProperty("version") +
      " " +
      Services.sysinfo.getProperty("build"),
    version: AppConstants.MOZ_APP_VERSION_DISPLAY,
    buildID: Services.appinfo.appBuildID,
    distributionID: Services.prefs
      .getDefaultBranch("")
      .getCharPref("distribution.id", ""),
    updateChannel: lazy.UpdateUtils.UpdateChannel,
    UpdRootD: Services.dirsvc.get("UpdRootD", Ci.nsIFile).path,
  };
  lazy.log.debug(`${SLUG}: current configuration`, data);

  // Other instances running are a transient precondition (during this invocation).  We'd prefer to
  // check this later, as a reason for not updating, but Glean is not tested in multi-process
  // environments and while its storage (backed by rkv) can in theory support multiple processes, it
  // is not clear that it in fact does support multiple processes.  So we are conservative here.
  // There is a potential time-of-check/time-of-use race condition here, but if process B starts
  // after we pass this test, that process should exit after it gets to this check, avoiding
  // multiple processes using the same Glean storage.  If and when more and longer-running
  // background tasks become common, we may need to be more fine-grained and share just the Glean
  // storage resource.
  lazy.log.debug(`${SLUG}: checking if other instance is running`);
  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );
  if (syncManager.isOtherInstanceRunning()) {
    lazy.log.error(`${SLUG}: another instance is running`);
    return EXIT_CODE.OTHER_INSTANCE;
  }

  // Here we mirror specific prefs from the default profile into our temporary profile.  We want to
  // do this early because some of the prefs may impact internals such as log levels.  Generally,
  // however, we want prefs from the default profile to not impact the mechanics of checking for,
  // downloading, and applying updates, since such prefs should be be per-installation prefs, using
  // the mechanisms of Bug 1691486.  Sadly using this mechanism for many relevant prefs (namely
  // `app.update.BITS.enabled` and `app.update.service.enabled`) is difficult: see Bug 1657533.
  //
  // We also read any Nimbus targeting snapshot from the default profile.
  let defaultProfileTargetingSnapshot = {};
  try {
    let defaultProfilePrefs;
    await lazy.BackgroundTasksUtils.withProfileLock(async lock => {
      let predicate = name => {
        return (
          name.startsWith("app.update.") || // For obvious reasons.
          name.startsWith("datareporting.") || // For Glean.
          name.startsWith("logging.") || // For Glean.
          name.startsWith("telemetry.fog.") || // For Glean.
          name.startsWith("app.partner.") || // For our metrics.
          name === "app.shield.optoutstudies.enabled" || // For Nimbus.
          name === "services.settings.server" || // For Remote Settings via Nimbus.
          name === "services.settings.preview_enabled" || // For Remote Settings via Nimbus.
          name === "messaging-system.rsexperimentloader.collection_id" // For Firefox Messaging System.
        );
      };

      defaultProfilePrefs = await lazy.BackgroundTasksUtils.readPreferences(
        predicate,
        lock
      );
      let telemetryClientID = await lazy.BackgroundTasksUtils.readTelemetryClientID(
        lock
      );
      Glean.backgroundUpdate.clientId.set(telemetryClientID);

      try {
        defaultProfileTargetingSnapshot = await lazy.BackgroundTasksUtils.readFirefoxMessagingSystemTargetingSnapshot(
          lock
        );
      } catch (f) {
        if (DOMException.isInstance(f) && f.name === "NotFoundError") {
          lazy.log.info(
            `${SLUG}: no default profile targeting snapshot exists`
          );
        } else {
          lazy.log.warn(
            `${SLUG}: ignoring exception reading default profile targeting snapshot`,
            f
          );
        }
      }
    });

    for (let [name, value] of Object.entries(defaultProfilePrefs)) {
      switch (typeof value) {
        case "boolean":
          Services.prefs.setBoolPref(name, value);
          break;
        case "number":
          Services.prefs.setIntPref(name, value);
          break;
        case "string":
          Services.prefs.setCharPref(name, value);
          break;
        default:
          throw new Error(
            `Pref from default profile with name "${name}" has unrecognized type`
          );
      }
    }
  } catch (e) {
    if (!lazy.BackgroundTasksUtils.hasDefaultProfile()) {
      lazy.log.error(`${SLUG}: caught exception; no default profile exists`, e);
      return EXIT_CODE.DEFAULT_PROFILE_DOES_NOT_EXIST;
    }

    if (e.name == "CannotLockProfileError") {
      lazy.log.error(
        `${SLUG}: caught exception; could not lock default profile`,
        e
      );
      return EXIT_CODE.DEFAULT_PROFILE_CANNOT_BE_LOCKED;
    }

    lazy.log.error(
      `${SLUG}: caught exception reading preferences and telemetry client ID from default profile`,
      e
    );
    return EXIT_CODE.DEFAULT_PROFILE_CANNOT_BE_READ;
  }

  // Now that we have prefs from the default profile, we can configure Firefox-on-Glean.

  // Glean has a preinit queue for metric operations that happen before init, so
  // this is safe.  We want to have these metrics set before the first possible
  // time we might send (built-in) pings.
  await BackgroundUpdate.recordUpdateEnvironment();

  // The final leaf is for the benefit of `FileUtils`.  To help debugging, use
  // the `GLEAN_LOG_PINGS` and `GLEAN_DEBUG_VIEW_TAG` environment variables: see
  // https://mozilla.github.io/glean/book/user/debugging/index.html.
  let gleanRoot = lazy.FileUtils.getFile("UpdRootD", [
    "backgroundupdate",
    "datareporting",
    "glean",
    "__dummy__",
  ]).parent.path;
  Services.fog.initializeFOG(gleanRoot, "firefox.desktop.background.update");

  // For convenience, mirror our loglevel.
  let logLevel = Services.prefs.getCharPref(
    "app.update.background.loglevel",
    "error"
  );
  const logLevelPrefs = [
    "browser.newtabpage.activity-stream.asrouter.debugLogLevel",
    "messaging-system.log",
    "services.settings.loglevel",
    "toolkit.backgroundtasks.loglevel",
  ];
  for (let logLevelPref of logLevelPrefs) {
    lazy.log.info(`${SLUG}: setting ${logLevelPref}=${logLevel}`);
    Services.prefs.setCharPref(logLevelPref, logLevel);
  }

  // The langpack updating mechanism expects the addons manager, but in background task mode, the
  // addons manager is not present.  Since we can't update langpacks from the background task
  // temporary profile, we disable the langpack updating mechanism entirely.  This relies on the
  // default profile being the only profile that schedules the OS-level background task and ensuring
  // the task is not scheduled when langpacks are present.  Non-default profiles that have langpacks
  // installed may experience the issues that motivated Bug 1647443.  If this turns out to be a
  // significant problem in the wild, we could store more information about profiles and their
  // active langpacks to disable background updates in more cases, maybe in per-installation prefs.
  Services.prefs.setBoolPref("app.update.langpack.enabled", false);

  let result = EXIT_CODE.SUCCESS;

  let stringStatus = lazy.AppUpdater.STATUS.debugStringFor(
    lazy.AppUpdater.STATUS.NEVER_CHECKED
  );
  Glean.backgroundUpdate.states.add(stringStatus);
  Glean.backgroundUpdate.finalState.set(stringStatus);

  try {
    await _attemptBackgroundUpdate();

    lazy.log.info(`${SLUG}: attempted background update`);
    Glean.backgroundUpdate.exitCodeSuccess.set(true);

    try {
      // Now that we've pumped the update loop, we can start Nimbus and the Firefox Messaging System
      // and see if we should message the user.  This minimizes the risk of messaging impacting the
      // function of the background update system.
      await lazy.BackgroundTasksUtils.enableNimbus(
        commandLine,
        defaultProfileTargetingSnapshot.environment
      );

      await lazy.BackgroundTasksUtils.enableFirefoxMessagingSystem(
        defaultProfileTargetingSnapshot.environment
      );
    } catch (f) {
      // Try to make it easy to witness errors in this system.  We can pass through any exception
      // without disrupting (future) background updates.
      //
      // Most meaningful issues with the Nimbus/experiments system will be reported via Glean
      // events.
      lazy.log.warn(
        `${SLUG}: exception raised from Nimbus/Firefox Messaging System`,
        f
      );
      throw f;
    }
  } catch (e) {
    // TODO: in the future, we might want to classify failures into transient and persistent and
    // backoff the update task in the face of continuous persistent errors.
    lazy.log.error(`${SLUG}: caught exception attempting background update`, e);

    result = EXIT_CODE.EXCEPTION;
    Glean.backgroundUpdate.exitCodeException.set(true);
  } finally {
    // This is the point to report telemetry, assuming that the default profile's data reporting
    // configuration allows it.
    await maybeSubmitBackgroundUpdatePing();
  }

  // TODO: ensure the update service has persisted its state before we exit.  Bug 1700846.
  // TODO: ensure that Glean's upload mechanism is aware of Gecko shutdown.  Bug 1703572.
  await lazy.ExtensionUtils.promiseTimeout(500);

  return result;
}
