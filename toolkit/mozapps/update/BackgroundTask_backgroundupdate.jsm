/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  BackgroundTasksUtils: "resource://gre/modules/BackgroundTasksUtils.jsm",
  BackgroundUpdate: "resource://gre/modules/BackgroundUpdate.jsm",
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
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

  // Other instances running are a transient precondition (during this invocation).
  if (!reasons.length) {
    log.debug(`${SLUG}: checking if other instance is running`);
    let syncManager = Cc[
      "@mozilla.org/updates/update-sync-manager;1"
    ].getService(Ci.nsIUpdateSyncManager);
    if (syncManager.isOtherInstanceRunning()) {
      reasons = BackgroundUpdate.REASON.OTHER_INSTANCE;
    }
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

    let _appUpdaterListener = status => {
      let stringStatus = AppUpdater.STATUS.debugStringFor(status);
      if (AppUpdater.STATUS.isTerminalStatus(status)) {
        log.debug(
          `${SLUG}: background update transitioned to terminal status ${status}: ${stringStatus}`
        );
        appUpdater.removeListener(_appUpdaterListener);
        resolve(true);
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

async function runBackgroundTask() {
  let SLUG = "runBackgroundTask";
  log.info(`${SLUG}: backgroundupdate`);

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

  // Here we mirror specific prefs from the default profile into our temporary profile.  We want to
  // do this early because some of the prefs may impact internals such as log levels.  Generally,
  // however, we want prefs from the default profile to not impact the mechanics of checking for,
  // downloading, and applying updates, since such prefs should be be per-installation prefs, using
  // the mechanisms of Bug 1691486.  Sadly using this mechanism for many relevant prefs (namely
  // `app.update.BITS.enabled` and `app.update.service.enabled`) is difficult: see Bug 1657533.
  try {
    let defaultProfilePrefs = await BackgroundTasksUtils.readPreferences(name =>
      name.startsWith("app.update.")
    );
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
    log.error(
      `${SLUG}: caught exception reading preferences from default profile`,
      e
    );

    return EXIT_CODE.EXCEPTION;
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
  try {
    await _attemptBackgroundUpdate();

    log.info(`${SLUG}: attempted background update`);

    // TODO: ensure the update service has persisted its state before we exit.  Bug 1700846.
    await ExtensionUtils.promiseTimeout(500);
  } catch (e) {
    // TODO: in the future, we might want to classify failures into transient and persistent and
    // backoff the update task in the fast of continuous persistent errors.
    log.error(`${SLUG}: caught exception attempting background update`, e);

    result = EXIT_CODE.EXCEPTION;
  } finally {
    // TODO: this is the point to report telemetry (assuming that the default profile's data
    // reporting configuration allows it).  Bug 1654891.
  }

  return result;
}
