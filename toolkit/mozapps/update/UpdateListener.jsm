/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UpdateListener"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { clearTimeout, setTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AppMenuNotifications",
  "resource://gre/modules/AppMenuNotifications.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "AppUpdateService",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);

const PREF_APP_UPDATE_UNSUPPORTED_URL = "app.update.unsupported.url";

// Setup the hamburger button badges for updates.
var UpdateListener = {
  timeouts: [],

  restartDoorhangerShown: false,
  // Once a restart badge/doorhanger is scheduled, these store the time that
  // they were scheduled at (as milliseconds elapsed since the UNIX epoch). This
  // allows us to resume the badge/doorhanger timers rather than restarting
  // them from the beginning when a new update comes along.
  updateFirstReadyTime: null,

  get badgeWaitTime() {
    return Services.prefs.getIntPref("app.update.badgeWaitTime", 4 * 24 * 3600); // 4 days
  },

  init() {
    // Persist the unsupported notification across sessions. If at some point an
    // update is found this pref is cleared and the notification won't be shown.
    let url = Services.prefs.getCharPref(PREF_APP_UPDATE_UNSUPPORTED_URL, null);
    if (url) {
      this.showUpdateNotification("unsupported", true, true, win =>
        this.openUnsupportedUpdateUrl(win, url)
      );
    }
  },

  uninit() {
    this.reset();
  },

  reset() {
    this.clearPendingAndActiveNotifications();
    this.restartDoorhangerShown = false;
    this.updateFirstReadyTime = null;
  },

  clearPendingAndActiveNotifications() {
    AppMenuNotifications.removeNotification(/^update-/);
    this.clearCallbacks();
  },

  clearCallbacks() {
    this.timeouts.forEach(t => clearTimeout(t));
    this.timeouts = [];
  },

  addTimeout(time, callback) {
    this.timeouts.push(
      setTimeout(() => {
        this.clearCallbacks();
        callback();
      }, time)
    );
  },

  requestRestart() {
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      "restart"
    );

    if (!cancelQuit.data) {
      Services.startup.quit(
        Services.startup.eAttemptQuit | Services.startup.eRestart
      );
    }
  },

  openManualUpdateUrl(win) {
    let manualUpdateUrl = Services.urlFormatter.formatURLPref(
      "app.update.url.manual"
    );
    win.openURL(manualUpdateUrl);
  },

  openUnsupportedUpdateUrl(win, detailsURL) {
    win.openURL(detailsURL);
  },

  showUpdateNotification(
    type,
    mainActionDismiss,
    dismissed,
    mainAction,
    beforeShowDoorhanger
  ) {
    const addTelemetry = id => {
      // No telemetry for the "downloading" state.
      if (type !== "downloading") {
        // Histogram category labels can't have dashes in them.
        let telemetryType = type.replaceAll("-", "");
        Services.telemetry.getHistogramById(id).add(telemetryType);
      }
    };
    let action = {
      callback(win, fromDoorhanger) {
        if (fromDoorhanger) {
          addTelemetry("UPDATE_NOTIFICATION_MAIN_ACTION_DOORHANGER");
        } else {
          addTelemetry("UPDATE_NOTIFICATION_MAIN_ACTION_MENU");
        }
        mainAction(win);
      },
      dismiss: mainActionDismiss,
    };

    let secondaryAction = {
      callback() {
        addTelemetry("UPDATE_NOTIFICATION_DISMISSED");
      },
      dismiss: true,
    };
    AppMenuNotifications.showNotification(
      "update-" + type,
      action,
      secondaryAction,
      { dismissed, beforeShowDoorhanger }
    );
    if (dismissed) {
      addTelemetry("UPDATE_NOTIFICATION_BADGE_SHOWN");
    } else {
      addTelemetry("UPDATE_NOTIFICATION_SHOWN");
    }
  },

  showRestartNotification(update, dismissed) {
    let notification = AppUpdateService.isOtherInstanceHandlingUpdates
      ? "other-instance"
      : "restart";
    if (!dismissed) {
      this.restartDoorhangerShown = true;
    }
    this.showUpdateNotification(notification, true, dismissed, () =>
      this.requestRestart()
    );
  },

  showUpdateAvailableNotification(update, dismissed) {
    this.showUpdateNotification("available", false, dismissed, () => {
      AppUpdateService.downloadUpdate(update, true);
    });
  },

  showManualUpdateNotification(update, dismissed) {
    this.showUpdateNotification("manual", false, dismissed, win =>
      this.openManualUpdateUrl(win)
    );
  },

  showUnsupportedUpdateNotification(update, dismissed) {
    if (!update || !update.detailsURL) {
      Cu.reportError(
        "The update for an unsupported notification must have a " +
          "detailsURL attribute."
      );
      return;
    }
    let url = update.detailsURL;
    if (
      url != Services.prefs.getCharPref(PREF_APP_UPDATE_UNSUPPORTED_URL, null)
    ) {
      Services.prefs.setCharPref(PREF_APP_UPDATE_UNSUPPORTED_URL, url);
      this.showUpdateNotification("unsupported", true, dismissed, win =>
        this.openUnsupportedUpdateUrl(win, url)
      );
    }
  },

  showUpdateDownloadingNotification() {
    this.showUpdateNotification("downloading", true, true, () => {
      // The user clicked on the "Downloading update" app menu item.
      // Code in browser/components/customizableui/content/panelUI.js
      // receives the following notification and opens the about dialog.
      Services.obs.notifyObservers(null, "show-update-progress");
    });
  },

  handleUpdateError(update, status) {
    switch (status) {
      case "download-attempt-failed":
        this.clearCallbacks();
        this.showUpdateAvailableNotification(update, false);
        break;
      case "download-attempts-exceeded":
        this.clearCallbacks();
        this.showManualUpdateNotification(update, false);
        break;
      case "elevation-attempt-failed":
        this.clearCallbacks();
        this.showRestartNotification(false);
        break;
      case "elevation-attempts-exceeded":
        this.clearCallbacks();
        this.showManualUpdateNotification(update, false);
        break;
      case "check-attempts-exceeded":
      case "unknown":
      case "bad-perms":
        // Background update has failed, let's show the UI responsible for
        // prompting the user to update manually.
        this.clearCallbacks();
        this.showManualUpdateNotification(update, false);
        break;
    }
  },

  handleUpdateStagedOrDownloaded(update, status) {
    switch (status) {
      case "applied":
      case "pending":
      case "applied-service":
      case "pending-service":
      case "pending-elevate":
      case "success":
        this.clearCallbacks();

        let initialBadgeWaitTimeMs = this.badgeWaitTime * 1000;
        let initialDoorhangerWaitTimeMs = update.promptWaitTime * 1000;
        let now = Date.now();

        if (!this.updateFirstReadyTime) {
          this.updateFirstReadyTime = now;
        }

        let badgeWaitTimeMs = Math.max(
          0,
          this.updateFirstReadyTime + initialBadgeWaitTimeMs - now
        );
        let doorhangerWaitTimeMs = Math.max(
          0,
          this.updateFirstReadyTime + initialDoorhangerWaitTimeMs - now
        );

        if (badgeWaitTimeMs < doorhangerWaitTimeMs) {
          this.addTimeout(badgeWaitTimeMs, () => {
            // Skip the badge if we're waiting for another instance.
            if (!AppUpdateService.isOtherInstanceHandlingUpdates) {
              this.showRestartNotification(update, true);
            }

            if (!this.restartDoorhangerShown) {
              // doorhangerWaitTimeMs is relative to when we initially received
              // the event. Since we've already waited badgeWaitTimeMs, subtract
              // that from doorhangerWaitTimeMs.
              let remainingTime = doorhangerWaitTimeMs - badgeWaitTimeMs;
              this.addTimeout(remainingTime, () => {
                this.showRestartNotification(update, false);
              });
            }
          });
        } else {
          this.addTimeout(doorhangerWaitTimeMs, () => {
            this.showRestartNotification(update, this.restartDoorhangerShown);
          });
        }
        break;
    }
  },

  handleUpdateAvailable(update, status) {
    switch (status) {
      case "show-prompt":
        // If an update is available and the app.update.auto preference is
        // false, then show an update available doorhanger.
        this.showUpdateAvailableNotification(update, false);
        break;
      case "cant-apply":
        this.clearCallbacks();
        this.showManualUpdateNotification(update, false);
        break;
      case "unsupported":
        this.clearCallbacks();
        this.showUnsupportedUpdateNotification(update, false);
        break;
    }
  },

  handleUpdateDownloading(status) {
    switch (status) {
      case "downloading":
        this.showUpdateDownloadingNotification();
        break;
      case "idle":
        this.clearPendingAndActiveNotifications();
        break;
    }
  },

  handleUpdateSwap() {
    // This function is called because we just finished downloading an update
    // (possibly) when another update was already ready.
    // At some point, we may want to have some sort of intermediate
    // notification to display here so that the badge doesn't just disappear.
    // Currently, this function just hides update notifications and clears
    // the callback timers so that notifications will not be shown. We want to
    // clear the restart notification so the user doesn't try to restart to
    // update during staging. We want to clear any other notifications too,
    // since none of them make sense to display now.
    // Our observer will fire again when the update is either ready to install
    // or an error has been encountered.
    this.clearPendingAndActiveNotifications();
  },

  observe(subject, topic, status) {
    let update = subject && subject.QueryInterface(Ci.nsIUpdate);

    switch (topic) {
      case "update-available":
        if (status != "unsupported") {
          // An update check has found an update so clear the unsupported pref
          // in case it is set.
          Services.prefs.clearUserPref(PREF_APP_UPDATE_UNSUPPORTED_URL);
        }
        this.handleUpdateAvailable(update, status);
        break;
      case "update-downloading":
        this.handleUpdateDownloading(status);
        break;
      case "update-staged":
      case "update-downloaded":
        // An update check has found an update and downloaded / staged the
        // update so clear the unsupported pref in case it is set.
        Services.prefs.clearUserPref(PREF_APP_UPDATE_UNSUPPORTED_URL);
        this.handleUpdateStagedOrDownloaded(update, status);
        break;
      case "update-error":
        this.handleUpdateError(update, status);
        break;
      case "update-swap":
        this.handleUpdateSwap();
        break;
    }
  },
};
