/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Update Prompt.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Alex Pakhotin <alexp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const UPDATE_NOTIFICATION_NAME = "update-app";
const UPDATE_NOTIFICATION_ICON = "drawable://alert_download_progress";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");


XPCOMUtils.defineLazyGetter(this, "gUpdateBundle", function aus_gUpdateBundle() {
  return Services.strings.createBundle("chrome://mozapps/locale/update/updates.properties");
});

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function aus_gBrandBundle() {
  return Services.strings.createBundle("chrome://branding/locale/brand.properties");
});

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function aus_gBrowserBundle() {
  return Services.strings.createBundle("chrome://browser/locale/browser.properties");
});

XPCOMUtils.defineLazyGetter(this, "AddonManager", function() {
  Cu.import("resource://gre/modules/AddonManager.jsm");
  return AddonManager;
});

XPCOMUtils.defineLazyGetter(this, "LocaleRepository", function() {
  Cu.import("resource://gre/modules/LocaleRepository.jsm");
  return LocaleRepository;
});

function getPref(func, preference, defaultValue) {
  try {
    return Services.prefs[func](preference);
  } catch (e) {}
  return defaultValue;
}

// -----------------------------------------------------------------------
// Update Prompt
// -----------------------------------------------------------------------

function UpdatePrompt() { }

UpdatePrompt.prototype = {
  classID: Components.ID("{88b3eb21-d072-4e3b-886d-f89d8c49fe59}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdatePrompt, Ci.nsIRequestObserver, Ci.nsIProgressEventSink]),

  get _enabled() {
    return !getPref("getBoolPref", "app.update.silent", false);
  },

  _showNotification: function UP__showNotif(aUpdate, aTitle, aText, aImageUrl, aMode) {
    let observer = {
      updatePrompt: this,
      observe: function (aSubject, aTopic, aData) {
        switch (aTopic) {
          case "alertclickcallback":
            this.updatePrompt._handleUpdate(aUpdate, aMode);
            break;
        }
      }
    };

    let notifier = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    notifier.showAlertNotification(aImageUrl, aTitle, aText, true, "", observer, UPDATE_NOTIFICATION_NAME);
  },

  _handleUpdate: function UP__handleUpdate(aUpdate, aMode) {
    if (aMode == "available") {
      let window = Services.wm.getMostRecentWindow("navigator:browser");
      let title = gUpdateBundle.GetStringFromName("updatesfound_" + aUpdate.type + ".title");
      let brandName = gBrandBundle.GetStringFromName("brandShortName");

      // Unconditionally use the "major" type here as for now it is always a new version
      // without additional description required for a minor update message
      let message = gUpdateBundle.formatStringFromName("intro_major", [brandName, aUpdate.displayVersion], 2);
      let button0 = gUpdateBundle.GetStringFromName("okButton");
      let button1 = gUpdateBundle.GetStringFromName("askLaterButton");
      let prompt = Services.prompt;
      let flags = prompt.BUTTON_POS_0 * prompt.BUTTON_TITLE_IS_STRING + prompt.BUTTON_POS_1 * prompt.BUTTON_TITLE_IS_STRING;

      let download = (prompt.confirmEx(window, title, message, flags, button0, button1, null, null, {value: false}) == 0);
      if (download) {
        // Start downloading the update in the background
        let aus = Cc["@mozilla.org/updates/update-service;1"].getService(Ci.nsIApplicationUpdateService);
        if (aus.downloadUpdate(aUpdate, true) != "failed") {
          let title = gBrowserBundle.formatStringFromName("alertDownloadsStart", [aUpdate.name], 1);
          this._showNotification(aUpdate, title, "", UPDATE_NOTIFICATION_ICON, "download");

          // Add this UI as a listener for active downloads
          aus.addDownloadListener(this);
        }
      }
    } else if(aMode == "downloaded") {
      // Notify all windows that an application quit has been requested
      let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

      // If nothing aborted, restart the app
      if (cancelQuit.data == false) {
        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
      }
    }
  },

  _updateDownloadProgress: function UP__updateDownloadProgress(aProgress, aTotal) {
    let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
    if (progressListener)
      progressListener.onProgress(UPDATE_NOTIFICATION_NAME, aProgress, aTotal);
  },

  // -------------------------
  // nsIUpdatePrompt interface
  // -------------------------

  // Right now this is used only to check for updates in progress
  checkForUpdates: function UP_checkForUpdates() {
    if (!this._enabled)
      return;

    let aus = Cc["@mozilla.org/updates/update-service;1"].getService(Ci.nsIApplicationUpdateService);
    if (!aus.isDownloading)
      return;

    let updateManager = Cc["@mozilla.org/updates/update-manager;1"].getService(Ci.nsIUpdateManager);

    let updateName = updateManager.activeUpdate ? updateManager.activeUpdate.name : gBrandBundle.GetStringFromName("brandShortName");
    let title = gBrowserBundle.formatStringFromName("alertDownloadsStart", [updateName], 1);

    this._showNotification(updateManager.activeUpdate, title, "", UPDATE_NOTIFICATION_ICON, "downloading");

    aus.removeDownloadListener(this); // just in case it's already added
    aus.addDownloadListener(this);
  },

  showUpdateAvailable: function UP_showUpdateAvailable(aUpdate) {
    if (!this._enabled)
      return;

    const PREF_APP_UPDATE_SKIPNOTIFICATION = "app.update.skipNotification";

    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SKIPNOTIFICATION) &&
        Services.prefs.getBoolPref(PREF_APP_UPDATE_SKIPNOTIFICATION)) {
      Services.prefs.setBoolPref(PREF_APP_UPDATE_SKIPNOTIFICATION, false);

      // Notification was already displayed and clicked, so jump to the next step:
      // ask the user about downloading update
      this._handleUpdate(aUpdate, "available");
      return;
    }

    let stringsPrefix = "updateAvailable_" + aUpdate.type + ".";
    let title = gUpdateBundle.formatStringFromName(stringsPrefix + "title", [aUpdate.name], 1);
    let text = gUpdateBundle.GetStringFromName(stringsPrefix + "text");
    let imageUrl = "";
    this._showNotification(aUpdate, title, text, imageUrl, "available");
  },

  showUpdateDownloaded: function UP_showUpdateDownloaded(aUpdate, aBackground) {
    if (!this._enabled)
      return;

    // uninstall all installed locales
    AddonManager.getAddonsByTypes(["locale"], (function (aAddons) {
      if (aAddons.length > 0) {
        let listener = this.getAddonListener(aUpdate, this);
        AddonManager.addAddonListener(listener);  
        aAddons.forEach(function(aAddon) {
          listener._uninstalling.push(aAddon.id);
          aAddon.uninstall();
        }, this);
      } else {
        this._showDownloadedNotification(aUpdate);
      }
    }).bind(this));
  },

  _showDownloadedNotification: function UP_showDlNotification(aUpdate) {
    let stringsPrefix = "updateDownloaded_" + aUpdate.type + ".";
    let title = gUpdateBundle.formatStringFromName(stringsPrefix + "title", [aUpdate.name], 1);
    let text = gUpdateBundle.GetStringFromName(stringsPrefix + "text");
    let imageUrl = "";
    this._showNotification(aUpdate, title, text, imageUrl, "downloaded");
  },

  _uninstalling: [],
  _installing: [],
  _currentUpdate: null,

  _reinstallLocales: function UP_reinstallLocales(aUpdate, aListener, aPending) {
    LocaleRepository.getLocales((function(aLocales) {
      aLocales.forEach(function(aLocale, aIndex, aArray) {
        let index = aPending.indexOf(aLocale.addon.id);
        if (index > -1) {
          aListener._installing.push(aLocale.addon.id);
          aLocale.addon.install.install();
        }
      }, this);
      // store the buildid of these locales so that we can disable locales when the
      // user updates through a non-updater channel
      Services.prefs.setCharPref("extensions.compatability.locales.buildid", aUpdate.buildID);
    }).bind(this), { buildID: aUpdate.buildID });
  },

  showUpdateInstalled: function UP_showUpdateInstalled() {
    if (!this._enabled || !getPref("getBoolPref", "app.update.showInstalledUI", false))
      return;

    let title = gBrandBundle.GetStringFromName("brandShortName");
    let text = gUpdateBundle.GetStringFromName("installSuccess");
    let imageUrl = "";
    this._showNotification(aUpdate, title, text, imageUrl, "installed");
  },

  showUpdateError: function UP_showUpdateError(aUpdate) {
    if (!this._enabled)
      return;

    if (aUpdate.state == "failed") {
      var title = gBrandBundle.GetStringFromName("brandShortName");
      let text = gUpdateBundle.GetStringFromName("updaterIOErrorTitle");
      let imageUrl = "";
      this._showNotification(aUpdate, title, text, imageUrl, "error");
    }
  },

  showUpdateHistory: function UP_showUpdateHistory(aParent) {
    // NOT IMPL
  },
  
  // ----------------------------
  // nsIRequestObserver interface
  // ----------------------------
  
  // When the data transfer begins
  onStartRequest: function(request, context) {
    // NOT IMPL
  },

  // When the data transfer ends
  onStopRequest: function(request, context, status) {
    let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
    if (progressListener)
      progressListener.onCancel(UPDATE_NOTIFICATION_NAME);


    let aus = Cc["@mozilla.org/updates/update-service;1"].getService(Ci.nsIApplicationUpdateService);
    aus.removeDownloadListener(this);
  },

  // ------------------------------
  // nsIProgressEventSink interface
  // ------------------------------
  
  // When new data has been downloaded
  onProgress: function(request, context, progress, maxProgress) {
    this._updateDownloadProgress(progress, maxProgress);
  },

  // When we have new status text
  onStatus: function(request, context, status, statusText) {
    // NOT IMPL
  },

  // -------------------------------
  // AddonListener
  // -------------------------------
  getAddonListener: function(aUpdate, aUpdatePrompt) {
    return {
      _installing: [],
      _uninstalling: [],
      onInstalling: function(aAddon, aNeedsRestart) {
        let index = this._installing.indexOf(aAddon.id);
        if (index > -1)
          this._installing.splice(index, 1);
    
        if (this._installing.length == 0) {
          aUpdatePrompt._showDownloadedNotification(aUpdate);
          AddonManager.removeAddonListener(this);
        }
      },
    
      onUninstalling: function(aAddon, aNeedsRestart) {
        let pending = [];
        let index = this._uninstalling.indexOf(aAddon.id);
        if (index > -1) {
          pending.push(aAddon.id);
          this._uninstalling.splice(index, 1);
        }
        if (this._uninstalling.length == 0)
          aUpdatePrompt._reinstallLocales(aUpdate, this, pending);
      }
    }
  }

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([UpdatePrompt]);
