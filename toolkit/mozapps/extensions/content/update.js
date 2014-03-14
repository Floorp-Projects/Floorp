// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This UI is only opened from the Extension Manager when the app is upgraded.

"use strict";

const PREF_UPDATE_EXTENSIONS_ENABLED            = "extensions.update.enabled";
const PREF_XPINSTALL_ENABLED                    = "xpinstall.enabled";
const PREF_EM_HOTFIX_ID                         = "extensions.hotfix.id";

// timeout (in milliseconds) to wait for response to the metadata ping
const METADATA_TIMEOUT    = 30000;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/addons/AddonRepository.jsm");

Components.utils.import("resource://gre/modules/Log.jsm");
let logger = Log.repository.getLogger("addons.update-dialog");

var gUpdateWizard = {
  // When synchronizing app compatibility info this contains all installed
  // add-ons. When checking for compatible versions this contains only
  // incompatible add-ons.
  addons: [],
  // Contains a list of add-ons that were disabled prior to the application
  // upgrade.
  inactiveAddonIDs: [],
  // The add-ons that we found updates available for
  addonsToUpdate: [],
  shouldSuggestAutoChecking: false,
  shouldAutoCheck: false,
  xpinstallEnabled: true,
  xpinstallLocked: false,
  // cached AddonInstall entries for add-ons we might want to update,
  // keyed by add-on ID
  addonInstalls: new Map(),
  shuttingDown: false,

  init: function gUpdateWizard_init()
  {
    this.inactiveAddonIDs = window.arguments[0];

    try {
      this.shouldSuggestAutoChecking =
        !Services.prefs.getBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED);
    }
    catch (e) {
    }

    try {
      this.xpinstallEnabled = Services.prefs.getBoolPref(PREF_XPINSTALL_ENABLED);
      this.xpinstallLocked = Services.prefs.prefIsLocked(PREF_XPINSTALL_ENABLED);
    }
    catch (e) {
    }

    if (Services.io.offline)
      document.documentElement.currentPage = document.getElementById("offline");
    else
      document.documentElement.currentPage = document.getElementById("versioninfo");
  },

  onWizardFinish: function gUpdateWizard_onWizardFinish ()
  {
    if (this.shouldSuggestAutoChecking)
      Services.prefs.setBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED, this.shouldAutoCheck);
  },

  _setUpButton: function gUpdateWizard_setUpButton(aButtonID, aButtonKey, aDisabled)
  {
    var strings = document.getElementById("updateStrings");
    var button = document.documentElement.getButton(aButtonID);
    if (aButtonKey) {
      button.label = strings.getString(aButtonKey);
      try {
        button.setAttribute("accesskey", strings.getString(aButtonKey + "Accesskey"));
      }
      catch (e) {
      }
    }
    button.disabled = aDisabled;
  },

  setButtonLabels: function gUpdateWizard_setButtonLabels(aBackButton, aBackButtonIsDisabled,
                             aNextButton, aNextButtonIsDisabled,
                             aCancelButton, aCancelButtonIsDisabled)
  {
    this._setUpButton("back", aBackButton, aBackButtonIsDisabled);
    this._setUpButton("next", aNextButton, aNextButtonIsDisabled);
    this._setUpButton("cancel", aCancelButton, aCancelButtonIsDisabled);
  },

  /////////////////////////////////////////////////////////////////////////////
  // Update Errors
  errorItems: [],

  checkForErrors: function gUpdateWizard_checkForErrors(aElementIDToShow)
  {
    if (this.errorItems.length > 0)
      document.getElementById(aElementIDToShow).hidden = false;
  },

  onWizardClose: function gUpdateWizard_onWizardClose(aEvent)
  {
    return this.onWizardCancel();
  },

  onWizardCancel: function gUpdateWizard_onWizardCancel()
  {
    gUpdateWizard.shuttingDown = true;
    // Allow add-ons to continue downloading and installing
    // in the background, though some may require a later restart
    // Pages that are waiting for user input go into the background
    // on cancel
    if (gMismatchPage.waiting) {
      logger.info("Dialog closed in mismatch page");
      if (gUpdateWizard.addonInstalls.size > 0) {
        gInstallingPage.startInstalls([i for ([, i] of gUpdateWizard.addonInstalls)]);
      }
      return true;
    }

    // Pages that do asynchronous things will just keep running and check
    // gUpdateWizard.shuttingDown to trigger background behaviour
    if (!gInstallingPage.installing) {
      logger.info("Dialog closed while waiting for updated compatibility information");
    }
    else {
      logger.info("Dialog closed while downloading and installing updates");
    }
    return true;
  }
};

var gOfflinePage = {
  onPageAdvanced: function gOfflinePage_onPageAdvanced()
  {
    Services.io.offline = false;
    return true;
  },

  toggleOffline: function gOfflinePage_toggleOffline()
  {
    var nextbtn = document.documentElement.getButton("next");
    nextbtn.disabled = !nextbtn.disabled;
  }
}

var gVersionInfoPage = {
  _completeCount: 0,
  _totalCount: 0,
  _versionInfoDone: false,
  onPageShow: function gVersionInfoPage_onPageShow()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "nextButtonText", true,
                                  "cancelButtonText", false);

    try {
      var hotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
    }
    catch (e) { }

    // Retrieve all add-ons in order to sync their app compatibility information
    AddonManager.getAllAddons(function gVersionInfoPage_getAllAddons(aAddons) {
      if (gUpdateWizard.shuttingDown) {
        logger.debug("getAllAddons completed after dialog closed");
      }

      gUpdateWizard.addons = [a for (a of aAddons)
                               if (a.type != "plugin" && a.id != hotfixID)];

      gVersionInfoPage._totalCount = gUpdateWizard.addons.length;

      // Ensure compatibility overrides are up to date before checking for
      // individual addon updates.
      let ids = [addon.id for (addon of gUpdateWizard.addons)];

      AddonRepository.repopulateCache(ids, function gVersionInfoPage_repopulateCache() {

        if (gUpdateWizard.shuttingDown) {
          logger.debug("repopulateCache completed after dialog closed");
        }

        for (let addon of gUpdateWizard.addons) {
          logger.debug("VersionInfo Finding updates for " + addon.id);
          addon.findUpdates(gVersionInfoPage, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
        }
      }, METADATA_TIMEOUT);
    });
  },

  onAllUpdatesFinished: function gVersionInfoPage_onAllUpdatesFinished() {
    // Filter out any add-ons that were disabled before the application was
    // upgraded or are already compatible
    logger.debug("VersionInfo updates finished: inactive " +
         gUpdateWizard.inactiveAddonIDs.toSource() + " found " +
         [addon.id + ":" + addon.appDisabled for (addon of gUpdateWizard.addons)].toSource());
    let filteredAddons = [];
    for (let a of gUpdateWizard.addons) {
      if (a.appDisabled && gUpdateWizard.inactiveAddonIDs.indexOf(a.id) < 0) {
        logger.debug("Continuing with add-on " + a.id);
        filteredAddons.push(a);
      }
      else if (gUpdateWizard.addonInstalls.has(a.id)) {
        gUpdateWizard.addonInstalls.get(a.id).cancel();
        gUpdateWizard.addonInstalls.delete(a.id);
      }
    }
    gUpdateWizard.addons = filteredAddons;

    if (gUpdateWizard.shuttingDown) {
      // jump directly to updating auto-update add-ons in the background
      if (gUpdateWizard.addonInstalls.size > 0) {
        gInstallingPage.startInstalls([i for ([, i] of gUpdateWizard.addonInstalls)]);
      }
      return;
    }

    if (filteredAddons.length > 0) {
      if (!gUpdateWizard.xpinstallEnabled && gUpdateWizard.xpinstallLocked) {
        document.documentElement.currentPage = document.getElementById("adminDisabled");
        return;
      }
      document.documentElement.currentPage = document.getElementById("mismatch");
    }
    else {
      logger.info("VersionInfo: No updates require further action");
      // VersionInfo compatibility updates resolved all compatibility problems,
      // close this window and continue starting the application...
      //XXX Bug 314754 - We need to use setTimeout to close the window due to
      // the EM using xmlHttpRequest when checking for updates.
      setTimeout(close, 0);
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // UpdateListener
  onUpdateFinished: function gVersionInfoPage_onUpdateFinished(aAddon, status) {
    ++this._completeCount;

    if (status != AddonManager.UPDATE_STATUS_NO_ERROR) {
      logger.debug("VersionInfo update " + this._completeCount + " of " + this._totalCount +
           " failed for " + aAddon.id + ": " + status);
      gUpdateWizard.errorItems.push(aAddon);
    }
    else {
      logger.debug("VersionInfo update " + this._completeCount + " of " + this._totalCount +
           " finished for " + aAddon.id);
    }

    // If we're not in the background, just make a list of add-ons that have
    // updates available
    if (!gUpdateWizard.shuttingDown) {
      // If we're still in the update check window and the add-on is now active
      // then it won't have been disabled by startup
      if (aAddon.active)
        AddonManagerPrivate.removeStartupChange("disabled", aAddon.id);

      // Update the status text and progress bar
      var updateStrings = document.getElementById("updateStrings");
      var statusElt = document.getElementById("versioninfo.status");
      var statusString = updateStrings.getFormattedString("statusPrefix", [aAddon.name]);
      statusElt.setAttribute("value", statusString);

      // Update the status text and progress bar
      var progress = document.getElementById("versioninfo.progress");
      progress.mode = "normal";
      progress.value = Math.ceil((this._completeCount / this._totalCount) * 100);
    }

    if (this._completeCount == this._totalCount)
      this.onAllUpdatesFinished();
  },

  onUpdateAvailable: function gVersionInfoPage_onUpdateAvailable(aAddon, aInstall) {
    logger.debug("VersionInfo got an install for " + aAddon.id + ": " + aAddon.version);
    gUpdateWizard.addonInstalls.set(aAddon.id, aInstall);
  },
};

var gMismatchPage = {
  waiting: false,

  onPageShow: function gMismatchPage_onPageShow()
  {
    gMismatchPage.waiting = true;
    gUpdateWizard.setButtonLabels(null, true,
                                  "mismatchCheckNow", false,
                                  "mismatchDontCheck", false);
    document.documentElement.getButton("next").focus();

    var incompatible = document.getElementById("mismatch.incompatible");
    for (let addon of gUpdateWizard.addons) {
      var listitem = document.createElement("listitem");
      listitem.setAttribute("label", addon.name + " " + addon.version);
      incompatible.appendChild(listitem);
    }
  }
};

var gUpdatePage = {
  _totalCount: 0,
  _completeCount: 0,
  onPageShow: function gUpdatePage_onPageShow()
  {
    gMismatchPage.waiting = false;
    gUpdateWizard.setButtonLabels(null, true,
                                  "nextButtonText", true,
                                  "cancelButtonText", false);
    document.documentElement.getButton("next").focus();

    gUpdateWizard.errorItems = [];

    this._totalCount = gUpdateWizard.addons.length;
    for (let addon of gUpdateWizard.addons) {
      logger.debug("UpdatePage requesting update for " + addon.id);
      // Redundant call to find updates again here when we already got them
      // in the VersionInfo page: https://bugzilla.mozilla.org/show_bug.cgi?id=960597
      addon.findUpdates(this, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
    }
  },

  onAllUpdatesFinished: function gUpdatePage_onAllUpdatesFinished() {
    if (gUpdateWizard.shuttingDown)
      return;

    var nextPage = document.getElementById("noupdates");
    if (gUpdateWizard.addonsToUpdate.length > 0)
      nextPage = document.getElementById("found");
    document.documentElement.currentPage = nextPage;
  },

  /////////////////////////////////////////////////////////////////////////////
  // UpdateListener
  onUpdateAvailable: function gUpdatePage_onUpdateAvailable(aAddon, aInstall) {
    logger.debug("UpdatePage got an update for " + aAddon.id + ": " + aAddon.version);
    gUpdateWizard.addonsToUpdate.push(aInstall);
  },

  onUpdateFinished: function gUpdatePage_onUpdateFinished(aAddon, status) {
    if (status != AddonManager.UPDATE_STATUS_NO_ERROR)
      gUpdateWizard.errorItems.push(aAddon);

    ++this._completeCount;

    if (!gUpdateWizard.shuttingDown) {
      // Update the status text and progress bar
      var updateStrings = document.getElementById("updateStrings");
      var statusElt = document.getElementById("checking.status");
      var statusString = updateStrings.getFormattedString("statusPrefix", [aAddon.name]);
      statusElt.setAttribute("value", statusString);

      var progress = document.getElementById("checking.progress");
      progress.value = Math.ceil((this._completeCount / this._totalCount) * 100);
    }

    if (this._completeCount == this._totalCount)
      this.onAllUpdatesFinished()
  },
};

var gFoundPage = {
  onPageShow: function gFoundPage_onPageShow()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "installButtonText", false,
                                  null, false);

    var foundUpdates = document.getElementById("found.updates");
    var itemCount = gUpdateWizard.addonsToUpdate.length;
    for (let install of gUpdateWizard.addonsToUpdate) {
      let listItem = foundUpdates.appendItem(install.name + " " + install.version);
      listItem.setAttribute("type", "checkbox");
      listItem.setAttribute("checked", "true");
      listItem.install = install;
    }

    if (!gUpdateWizard.xpinstallEnabled) {
      document.getElementById("xpinstallDisabledAlert").hidden = false;
      document.getElementById("enableXPInstall").focus();
      document.documentElement.getButton("next").disabled = true;
    }
    else {
      document.documentElement.getButton("next").focus();
      document.documentElement.getButton("next").disabled = false;
    }
  },

  toggleXPInstallEnable: function gFoundPage_toggleXPInstallEnable(aEvent)
  {
    var enabled = aEvent.target.checked;
    gUpdateWizard.xpinstallEnabled = enabled;
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    pref.setBoolPref(PREF_XPINSTALL_ENABLED, enabled);
    this.updateNextButton();
  },

  updateNextButton: function gFoundPage_updateNextButton()
  {
    if (!gUpdateWizard.xpinstallEnabled) {
      document.documentElement.getButton("next").disabled = true;
      return;
    }

    var oneChecked = false;
    var foundUpdates = document.getElementById("found.updates");
    var updates = foundUpdates.getElementsByTagName("listitem");
    for (let update of updates) {
      if (!update.checked)
        continue;
      oneChecked = true;
      break;
    }

    gUpdateWizard.setButtonLabels(null, true,
                                  "installButtonText", true,
                                  null, false);
    document.getElementById("found").setAttribute("next", "installing");
    document.documentElement.getButton("next").disabled = !oneChecked;
  }
};

var gInstallingPage = {
  _installs         : [],
  _errors           : [],
  _strings          : null,
  _currentInstall   : -1,
  _installing       : false,

  // Initialize fields we need for installing and tracking progress,
  // and start iterating through the installations
  startInstalls: function gInstallingPage_startInstalls(aInstallList) {
    if (!gUpdateWizard.xpinstallEnabled) {
      return;
    }

    logger.debug("Start installs for "
                 + [i.existingAddon.id for (i of aInstallList)].toSource());
    this._errors = [];
    this._installs = aInstallList;
    this._installing = true;
    this.startNextInstall();
  },

  onPageShow: function gInstallingPage_onPageShow()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "nextButtonText", true,
                                  null, true);

    var foundUpdates = document.getElementById("found.updates");
    var updates = foundUpdates.getElementsByTagName("listitem");
    let toInstall = [];
    for (let update of updates) {
      if (!update.checked) {
        logger.info("User chose to cancel update of " + update.label);
        update.install.cancel();
        continue;
      }
      toInstall.push(update.install);
    }
    this._strings = document.getElementById("updateStrings");

    this.startInstalls(toInstall);
  },

  startNextInstall: function gInstallingPage_startNextInstall() {
    if (this._currentInstall >= 0) {
      this._installs[this._currentInstall].removeListener(this);
    }

    this._currentInstall++;

    if (this._installs.length == this._currentInstall) {
      Services.obs.notifyObservers(null, "TEST:all-updates-done", null);
      this._installing = false;
      if (gUpdateWizard.shuttingDown) {
        return;
      }
      var nextPage = this._errors.length > 0 ? "installerrors" : "finished";
      document.getElementById("installing").setAttribute("next", nextPage);
      document.documentElement.advance();
      return;
    }

    let install = this._installs[this._currentInstall];

    if (gUpdateWizard.shuttingDown && !AddonManager.shouldAutoUpdate(install.existingAddon)) {
      logger.debug("Don't update " + install.existingAddon.id + " in background");
      install.cancel();
      this.startNextInstall();
      return;
    }
    install.addListener(this);
    install.install();
  },

  /////////////////////////////////////////////////////////////////////////////
  // InstallListener
  onDownloadStarted: function gInstallingPage_onDownloadStarted(aInstall) {
    if (gUpdateWizard.shuttingDown) {
      return;
    }
    var strings = document.getElementById("updateStrings");
    var label = strings.getFormattedString("downloadingPrefix", [aInstall.name]);
    var actionItem = document.getElementById("actionItem");
    actionItem.value = label;
  },

  onDownloadProgress: function gInstallingPage_onDownloadProgress(aInstall) {
    if (gUpdateWizard.shuttingDown) {
      return;
    }
    var downloadProgress = document.getElementById("downloadProgress");
    downloadProgress.value = Math.ceil(100 * aInstall.progress / aInstall.maxProgress);
  },

  onDownloadEnded: function gInstallingPage_onDownloadEnded(aInstall) {
  },

  onDownloadFailed: function gInstallingPage_onDownloadFailed(aInstall) {
    this._errors.push(aInstall);

    this.startNextInstall();
  },

  onInstallStarted: function gInstallingPage_onInstallStarted(aInstall) {
    if (gUpdateWizard.shuttingDown) {
      return;
    }
    var strings = document.getElementById("updateStrings");
    var label = strings.getFormattedString("installingPrefix", [aInstall.name]);
    var actionItem = document.getElementById("actionItem");
    actionItem.value = label;
  },

  onInstallEnded: function gInstallingPage_onInstallEnded(aInstall, aAddon) {
    if (!gUpdateWizard.shuttingDown) {
      // Remember that this add-on was updated during startup
      AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED,
                                           aAddon.id);
    }

    this.startNextInstall();
  },

  onInstallFailed: function gInstallingPage_onInstallFailed(aInstall) {
    this._errors.push(aInstall);

    this.startNextInstall();
  }
};

var gInstallErrorsPage = {
  onPageShow: function gInstallErrorsPage_onPageShow()
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    document.documentElement.getButton("finish").focus();
  },
};

// Displayed when there are incompatible add-ons and the xpinstall.enabled
// pref is false and locked.
var gAdminDisabledPage = {
  onPageShow: function gAdminDisabledPage_onPageShow()
  {
    gUpdateWizard.setButtonLabels(null, true, null, true,
                                  "cancelButtonText", true);
    document.documentElement.getButton("finish").focus();
  }
};

// Displayed when selected add-on updates have been installed without error.
// There can still be add-ons that are not compatible and don't have an update.
var gFinishedPage = {
  onPageShow: function gFinishedPage_onPageShow()
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    document.documentElement.getButton("finish").focus();

    if (gUpdateWizard.shouldSuggestAutoChecking) {
      document.getElementById("finishedCheckDisabled").hidden = false;
      gUpdateWizard.shouldAutoCheck = true;
    }
    else
      document.getElementById("finishedCheckEnabled").hidden = false;

    document.documentElement.getButton("finish").focus();
  }
};

// Displayed when there are incompatible add-ons and there are no available
// updates.
var gNoUpdatesPage = {
  onPageShow: function gNoUpdatesPage_onPageLoad(aEvent)
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    if (gUpdateWizard.shouldSuggestAutoChecking) {
      document.getElementById("noupdatesCheckDisabled").hidden = false;
      gUpdateWizard.shouldAutoCheck = true;
    }
    else
      document.getElementById("noupdatesCheckEnabled").hidden = false;

    gUpdateWizard.checkForErrors("updateCheckErrorNotFound");
    document.documentElement.getButton("finish").focus();
  }
};
