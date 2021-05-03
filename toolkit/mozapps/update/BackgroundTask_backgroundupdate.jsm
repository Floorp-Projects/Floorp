/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundUpdate.jsm"
).BackgroundUpdate;
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  BackgroundTasksUtils: "resource://gre/modules/BackgroundTasksUtils.jsm",
  BackgroundUpdate: "resource://gre/modules/BackgroundUpdate.jsm",
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "UpdateService",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);

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

Cu.importGlobalProperties(["Glean", "GleanPings"]);

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
  log.debug(
    `${SLUG}: creating UpdateServiceStub() for "post-update-processing"`
  );
  Cc["@mozilla.org/updates/update-service-stub;1"].createInstance(
    Ci.nsISupports
  );

  log.debug(
    `${SLUG}: checking for preconditions necessary to update this installation`
  );
  let reasons = await BackgroundUpdate._reasonsToNotUpdateInstallation();

  if (BackgroundUpdate._force()) {
    // We want to allow developers and testers to monkey with the system.
    log.debug(
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
    log.info(
      `${SLUG}: not running background update task: '${JSON.stringify(
        reasons
      )}'`
    );

    return false;
  }

  let result = new Promise(resolve => {
    let appUpdater = new AppUpdater();

    let _appUpdaterListener = (status, progress, progressMax) => {
      let stringStatus = AppUpdater.STATUS.debugStringFor(status);
      Glean.backgroundUpdate.states.add(stringStatus);
      Glean.backgroundUpdate.finalState.set(stringStatus);

      if (AppUpdater.STATUS.isTerminalStatus(status)) {
        log.debug(
          `${SLUG}: background update transitioned to terminal status ${status}: ${stringStatus}`
        );
        appUpdater.removeListener(_appUpdaterListener);
        resolve(true);
      } else if (status == AppUpdater.STATUS.CHECKING) {
        // The usual initial flow for the Background Update Task is to kick off
        // the update download and immediately exit. For consistency, we are
        // going to enforce this flow. So if we are just now checking for
        // updates, we will limit the updater such that it cannot start staging,
        // even if we immediately download the entire update.
        log.debug(
          `${SLUG}: This session will be limited to downloading updates only.`
        );
        UpdateService.onlyDownloadUpdatesThisSession = true;
      } else if (
        status == AppUpdater.STATUS.DOWNLOADING &&
        (UpdateService.onlyDownloadUpdatesThisSession ||
          (progress !== undefined && progressMax !== undefined))
      ) {
        // We get a DOWNLOADING callback with no progress or progressMax values
        // when we initially switch to the DOWNLOADING state. But when we get
        // onProgress notifications, progress and progressMax will be defined.
        // Remember to keep in mind that progressMax is a required value that
        // we can count on being meaningful, but it will be set to -1 for BITS
        // transfers that haven't begun yet.
        if (
          UpdateService.onlyDownloadUpdatesThisSession ||
          progressMax < 0 ||
          progress != progressMax
        ) {
          log.debug(
            `${SLUG}: Download in progress. Exiting task while download ` +
              `transfers`
          );
          // If the download is still in progress, we don't want the Background
          // Update Task to hang around waiting for it to complete.
          UpdateService.onlyDownloadUpdatesThisSession = true;

          appUpdater.removeListener(_appUpdaterListener);
          resolve(true);
        } else {
          log.debug(`${SLUG}: Download has completed!`);
        }
      } else {
        log.debug(
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

  log.info(`${SLUG}: submitted "background-update" ping`);
}

async function runBackgroundTask() {
  let SLUG = "runBackgroundTask";
  log.error(`${SLUG}: backgroundupdate`);

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
    updateChannel: UpdateUtils.UpdateChannel,
    UpdRootD: Services.dirsvc.get("UpdRootD", Ci.nsIFile).path,
  };
  log.debug(`${SLUG}: current configuration`, data);

  // Other instances running are a transient precondition (during this invocation).  We'd prefer to
  // check this later, as a reason for not updating, but Glean is not tested in multi-process
  // environments and while its storage (backed by rkv) can in theory support multiple processes, it
  // is not clear that it in fact does support multiple processes.  So we are conservative here.
  // There is a potential time-of-check/time-of-use race condition here, but if process B starts
  // after we pass this test, that process should exit after it gets to this check, avoiding
  // multiple processes using the same Glean storage.  If and when more and longer-running
  // background tasks become common, we may need to be more fine-grained and share just the Glean
  // storage resource.
  log.debug(`${SLUG}: checking if other instance is running`);
  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );
  if (syncManager.isOtherInstanceRunning()) {
    log.error(`${SLUG}: another instance is running`);
    return EXIT_CODE.OTHER_INSTANCE;
  }

  // Here we mirror specific prefs from the default profile into our temporary profile.  We want to
  // do this early because some of the prefs may impact internals such as log levels.  Generally,
  // however, we want prefs from the default profile to not impact the mechanics of checking for,
  // downloading, and applying updates, since such prefs should be be per-installation prefs, using
  // the mechanisms of Bug 1691486.  Sadly using this mechanism for many relevant prefs (namely
  // `app.update.BITS.enabled` and `app.update.service.enabled`) is difficult: see Bug 1657533.
  try {
    let defaultProfilePrefs;
    await BackgroundTasksUtils.withProfileLock(async lock => {
      let predicate = name => {
        return (
          name.startsWith("app.update.") || // For obvious reasons.
          name.startsWith("datareporting.") || // For Glean.
          name.startsWith("logging.") || // For Glean.
          name.startsWith("telemetry.fog.") || // For Glean.
          name.startsWith("app.partner.") // For our metrics.
        );
      };

      defaultProfilePrefs = await BackgroundTasksUtils.readPreferences(
        predicate,
        lock
      );
      let telemetryClientID = await BackgroundTasksUtils.readTelemetryClientID(
        lock
      );
      Glean.backgroundUpdate.clientId.set(telemetryClientID);
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
    if (!BackgroundTasksUtils.hasDefaultProfile()) {
      log.error(`${SLUG}: caught exception; no default profile exists`, e);
      return EXIT_CODE.DEFAULT_PROFILE_DOES_NOT_EXIST;
    }

    if (e.name == "CannotLockProfileError") {
      log.error(`${SLUG}: caught exception; could not lock default profile`, e);
      return EXIT_CODE.DEFAULT_PROFILE_CANNOT_BE_LOCKED;
    }

    log.error(
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
  let gleanRoot = FileUtils.getFile("UpdRootD", [
    "backgroundupdate",
    "datareporting",
    "glean",
    "__dummy__",
  ]).parent.path;
  let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
  FOG.initializeFOG(gleanRoot, "firefox.desktop.background.update");

  // For convenience, mirror our loglevel.
  Services.prefs.setCharPref(
    "toolkit.backgroundtasks.loglevel",
    Services.prefs.getCharPref("app.update.background.loglevel", "error")
  );

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

  let stringStatus = AppUpdater.STATUS.debugStringFor(
    AppUpdater.STATUS.NEVER_CHECKED
  );
  Glean.backgroundUpdate.states.add(stringStatus);
  Glean.backgroundUpdate.finalState.set(stringStatus);

  try {
    await _attemptBackgroundUpdate();

    log.info(`${SLUG}: attempted background update`);
    Glean.backgroundUpdate.exitCodeSuccess.set(true);
  } catch (e) {
    // TODO: in the future, we might want to classify failures into transient and persistent and
    // backoff the update task in the face of continuous persistent errors.
    log.error(`${SLUG}: caught exception attempting background update`, e);

    result = EXIT_CODE.EXCEPTION;
    Glean.backgroundUpdate.exitCodeException.set(true);
  } finally {
    // This is the point to report telemetry, assuming that the default profile's data reporting
    // configuration allows it.
    await maybeSubmitBackgroundUpdatePing();
  }

  // TODO: ensure the update service has persisted its state before we exit.  Bug 1700846.
  // TODO: ensure that Glean's upload mechanism is aware of Gecko shutdown.  Bug 1703572.
  await ExtensionUtils.promiseTimeout(500);

  return result;
}
