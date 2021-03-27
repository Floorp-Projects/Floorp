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
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
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

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

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
