/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UpdateListener"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {clearTimeout, setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");

ChromeUtils.defineModuleGetter(this, "AppMenuNotifications",
                               "resource://gre/modules/AppMenuNotifications.jsm");

const PREF_APP_UPDATE_UNSUPPORTED_URL = "app.update.unsupported.url";

// Setup the hamburger button badges for updates, if enabled.
var UpdateListener = {
  timeouts: [],

  get enabled() {
    return Services.prefs.getBoolPref("app.update.doorhanger", false);
  },

  get badgeWaitTime() {
    return Services.prefs.getIntPref("app.update.badgeWaitTime", 4 * 24 * 3600); // 4 days
  },

  init() {
    // Persist the unsupported notification across sessions. If at some point an
    // update is found this pref is cleared and the notifcation won't be shown.
    let url = Services.prefs.getCharPref(PREF_APP_UPDATE_UNSUPPORTED_URL, null);
    if (url) {
      this.showUpdateNotification("unsupported", true, true,
                                  win => this.openUnsupportedUpdateUrl(win, url));
    }
  },

  uninit() {
    this.reset();
  },

  reset() {
    AppMenuNotifications.removeNotification(/^update-/);
    this.clearCallbacks();
  },

  clearCallbacks() {
    this.timeouts.forEach(t => clearTimeout(t));
    this.timeouts = [];
  },

  addTimeout(time, callback) {
    this.timeouts.push(setTimeout(() => {
      this.clearCallbacks();
      callback();
    }, time));
  },

  replaceReleaseNotes(doc, update, whatsNewId) {
    let whatsNewLinkId = Services.prefs.getCharPref(`app.update.link.${whatsNewId}`, "");
    if (whatsNewLinkId) {
      let whatsNewLink = doc.getElementById(whatsNewLinkId);
      if (update && update.detailsURL) {
        whatsNewLink.href = update.detailsURL;
      } else {
        whatsNewLink.href = Services.urlFormatter.formatURLPref("app.update.url.details");
      }
    }
  },

  requestRestart() {
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                     createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

    if (!cancelQuit.data) {
      Services.startup.quit(Services.startup.eAttemptQuit | Services.startup.eRestart);
    }
  },

  openManualUpdateUrl(win) {
    let manualUpdateUrl = Services.urlFormatter.formatURLPref("app.update.url.manual");
    win.openURL(manualUpdateUrl);
  },

  openUnsupportedUpdateUrl(win, detailsURL) {
    win.openURL(detailsURL);
  },

  showUpdateNotification(type, mainActionDismiss, dismissed, mainAction, beforeShowDoorhanger) {
    let action = {
      callback(win, fromDoorhanger) {
        if (fromDoorhanger) {
          Services.telemetry.getHistogramById("UPDATE_NOTIFICATION_MAIN_ACTION_DOORHANGER").add(type);
        } else {
          Services.telemetry.getHistogramById("UPDATE_NOTIFICATION_MAIN_ACTION_MENU").add(type);
        }
        mainAction(win);
      },
      dismiss: mainActionDismiss,
    };

    let secondaryAction = {
      callback() {
        Services.telemetry.getHistogramById("UPDATE_NOTIFICATION_DISMISSED").add(type);
      },
      dismiss: true,
    };

    AppMenuNotifications.showNotification("update-" + type,
                                          action,
                                          secondaryAction,
                                          { dismissed, beforeShowDoorhanger });
    if (dismissed) {
      Services.telemetry.getHistogramById("UPDATE_NOTIFICATION_BADGE_SHOWN").add(type);
    } else {
      Services.telemetry.getHistogramById("UPDATE_NOTIFICATION_SHOWN").add(type);
    }
  },

  showRestartNotification(dismissed) {
    this.showUpdateNotification("restart", true, dismissed, () => this.requestRestart());
  },

  showUpdateAvailableNotification(update, dismissed) {
    this.showUpdateNotification("available", false, dismissed, () => {
      let updateService = Cc["@mozilla.org/updates/update-service;1"]
                          .getService(Ci.nsIApplicationUpdateService);
      updateService.downloadUpdate(update, true);
    }, doc => this.replaceReleaseNotes(doc, update, "updateAvailableWhatsNew"));
  },

  showManualUpdateNotification(update, dismissed) {
    this.showUpdateNotification("manual",
                                false,
                                dismissed,
                                win => this.openManualUpdateUrl(win),
                                doc => this.replaceReleaseNotes(doc, update, "updateManualWhatsNew"));
  },

  showUnsupportedUpdateNotification(update, dismissed) {
    if (!update || !update.detailsURL) {
      Cu.reportError("The update for an unsupported notification must have a " +
                     "detailsURL attribute.");
      return;
    }
    let url = update.detailsURL;
    if (url != Services.prefs.getCharPref(PREF_APP_UPDATE_UNSUPPORTED_URL, null)) {
      Services.prefs.setCharPref(PREF_APP_UPDATE_UNSUPPORTED_URL, url);
      this.showUpdateNotification("unsupported", true, dismissed,
                                  win => this.openUnsupportedUpdateUrl(win, url));
    }
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

        let badgeWaitTimeMs = this.badgeWaitTime * 1000;
        let doorhangerWaitTimeMs = update.promptWaitTime * 1000;

        if (badgeWaitTimeMs < doorhangerWaitTimeMs) {
          this.addTimeout(badgeWaitTimeMs, () => {
            this.showRestartNotification(true);

            // doorhangerWaitTimeMs is relative to when we initially received
            // the event. Since we've already waited badgeWaitTimeMs, subtract
            // that from doorhangerWaitTimeMs.
            let remainingTime = doorhangerWaitTimeMs - badgeWaitTimeMs;
            this.addTimeout(remainingTime, () => {
              this.showRestartNotification(false);
            });
          });
        } else {
          this.addTimeout(doorhangerWaitTimeMs, () => {
            this.showRestartNotification(false);
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
        this.clearCallbacks();
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

  observe(subject, topic, status) {
    if (!this.enabled) {
      return;
    }

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
    }
  },
};
