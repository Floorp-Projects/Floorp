# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// This UI is only opened from the Extension Manager when the app is upgraded.

"use strict";

const PREF_UPDATE_EXTENSIONS_ENABLED            = "extensions.update.enabled";
const PREF_XPINSTALL_ENABLED                    = "xpinstall.enabled";
const PREF_EM_HOTFIX_ID                         = "extensions.hotfix.id";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/AddonRepository.jsm");


var gInteruptable = true;
var gPendingClose = false;


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

  init: function ()
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

  onWizardFinish: function ()
  {
    if (this.shouldSuggestAutoChecking)
      Services.prefs.setBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED, this.shouldAutoCheck);
  },

  _setUpButton: function (aButtonID, aButtonKey, aDisabled)
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

  setButtonLabels: function (aBackButton, aBackButtonIsDisabled,
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

  checkForErrors: function (aElementIDToShow)
  {
    if (this.errorItems.length > 0)
      document.getElementById(aElementIDToShow).hidden = false;
  },

  onWizardClose: function (aEvent)
  {
    return this.onWizardCancel();
  },

  onWizardCancel: function ()
  {
    if (!gInteruptable) {
      gPendingClose = true;
      this._setUpButton("back", null, true);
      this._setUpButton("next", null, true);
      this._setUpButton("cancel", null, true);
      return false;
    }

    if (gInstallingPage.installing) {
      gInstallingPage.cancelInstalls();
      return false;
    }
    return true;
  }
};

var gOfflinePage = {
  onPageAdvanced: function ()
  {
    Services.io.offline = false;
    return true;
  },

  toggleOffline: function ()
  {
    var nextbtn = document.documentElement.getButton("next");
    nextbtn.disabled = !nextbtn.disabled;
  }
}

var gVersionInfoPage = {
  _completeCount: 0,
  _totalCount: 0,
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "nextButtonText", true,
                                  "cancelButtonText", false);

    try {
      var hotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
    }
    catch (e) { }

    // Retrieve all add-ons in order to sync their app compatibility information
    AddonManager.getAllAddons(function(aAddons) {
      gUpdateWizard.addons = aAddons.filter(function(a) {
        return a.type != "plugin" && a.id != hotfixID;
      });

      gVersionInfoPage._totalCount = gUpdateWizard.addons.length;

      // Ensure compatibility overrides are up to date before checking for
      // individual addon updates.
      let ids = [addon.id for each (addon in gUpdateWizard.addons)];

      gInteruptable = false;
      AddonRepository.repopulateCache(ids, function() {
        AddonManagerPrivate.updateAddonRepositoryData(function() {
          gInteruptable = true;
          if (gPendingClose) {
            window.close();
            return;
          }

          gUpdateWizard.addons.forEach(function(aAddon) {
            aAddon.findUpdates(gVersionInfoPage, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
          }, this);
        });
      });
    });
  },

  onAllUpdatesFinished: function() {
    // Filter out any add-ons that were disabled before the application was
    // upgraded or are already compatible
    gUpdateWizard.addons = gUpdateWizard.addons.filter(function(a) {
      return a.appDisabled && gUpdateWizard.inactiveAddonIDs.indexOf(a.id) < 0;
    });

    if (gUpdateWizard.addons.length > 0) {
      // There are still incompatible addons, inform the user.
      document.documentElement.currentPage = document.getElementById("mismatch");
    }
    else {
      // VersionInfo compatibility updates resolved all compatibility problems,
      // close this window and continue starting the application...
      //XXX Bug 314754 - We need to use setTimeout to close the window due to
      // the EM using xmlHttpRequest when checking for updates.
      setTimeout(close, 0);
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // UpdateListener
  onUpdateFinished: function(aAddon, status) {
    // If the add-on is now active then it won't have been disabled by startup
    if (aAddon.active)
      AddonManagerPrivate.removeStartupChange("disabled", aAddon.id);

    if (status != AddonManager.UPDATE_STATUS_NO_ERROR)
      gUpdateWizard.errorItems.push(aAddon);

    ++this._completeCount;

    // Update the status text and progress bar
    var updateStrings = document.getElementById("updateStrings");
    var status = document.getElementById("versioninfo.status");
    var statusString = updateStrings.getFormattedString("statusPrefix", [aAddon.name]);
    status.setAttribute("value", statusString);

    // Update the status text and progress bar
    var progress = document.getElementById("versioninfo.progress");
    progress.mode = "normal";
    progress.value = Math.ceil((this._completeCount / this._totalCount) * 100);

    if (this._completeCount == this._totalCount)
      this.onAllUpdatesFinished();
  },
};

var gMismatchPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "mismatchCheckNow", false,
                                  "mismatchDontCheck", false);
    document.documentElement.getButton("next").focus();

    var incompatible = document.getElementById("mismatch.incompatible");
    gUpdateWizard.addons.forEach(function(aAddon) {
      var listitem = document.createElement("listitem");
      listitem.setAttribute("label", aAddon.name + " " + aAddon.version);
      incompatible.appendChild(listitem);
    });
  }
};

var gUpdatePage = {
  _totalCount: 0,
  _completeCount: 0,
  onPageShow: function ()
  {
    if (!gUpdateWizard.xpinstallEnabled && gUpdateWizard.xpinstallLocked) {
      document.documentElement.currentPage = document.getElementById("adminDisabled");
      return;
    }

    gUpdateWizard.setButtonLabels(null, true,
                                  "nextButtonText", true,
                                  "cancelButtonText", false);
    document.documentElement.getButton("next").focus();

    gUpdateWizard.errorItems = [];

    this._totalCount = gUpdateWizard.addons.length;
    gUpdateWizard.addons.forEach(function(aAddon) {
      aAddon.findUpdates(this, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
    }, this);
  },

  onAllUpdatesFinished: function() {
    var nextPage = document.getElementById("noupdates");
    if (gUpdateWizard.addonsToUpdate.length > 0)
      nextPage = document.getElementById("found");
    document.documentElement.currentPage = nextPage;
  },

  /////////////////////////////////////////////////////////////////////////////
  // UpdateListener
  onUpdateAvailable: function(aAddon, aInstall) {
    gUpdateWizard.addonsToUpdate.push(aInstall);
  },

  onUpdateFinished: function(aAddon, status) {
    if (status != AddonManager.UPDATE_STATUS_NO_ERROR)
      gUpdateWizard.errorItems.push(aAddon);

    ++this._completeCount;

    // Update the status text and progress bar
    var updateStrings = document.getElementById("updateStrings");
    var status = document.getElementById("checking.status");
    var statusString = updateStrings.getFormattedString("statusPrefix", [aAddon.name]);
    status.setAttribute("value", statusString);

    var progress = document.getElementById("checking.progress");
    progress.value = Math.ceil((this._completeCount / this._totalCount) * 100);

    if (this._completeCount == this._totalCount)
      this.onAllUpdatesFinished()
  },
};

var gFoundPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "installButtonText", false,
                                  null, false);

    var foundUpdates = document.getElementById("found.updates");
    var itemCount = gUpdateWizard.addonsToUpdate.length;
    gUpdateWizard.addonsToUpdate.forEach(function(aInstall) {
      var listItem = foundUpdates.appendItem(aInstall.name + " " + aInstall.version);
      listItem.setAttribute("type", "checkbox");
      listItem.setAttribute("checked", "true");
      listItem.install = aInstall;
    });

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

  toggleXPInstallEnable: function(aEvent)
  {
    var enabled = aEvent.target.checked;
    gUpdateWizard.xpinstallEnabled = enabled;
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    pref.setBoolPref(PREF_XPINSTALL_ENABLED, enabled);
    this.updateNextButton();
  },

  updateNextButton: function ()
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

  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "nextButtonText", true,
                                  null, true);
    this._errors = [];

    var foundUpdates = document.getElementById("found.updates");
    var updates = foundUpdates.getElementsByTagName("listitem");
    for (let update of updates) {
      if (!update.checked)
        continue;
      this._installs.push(update.install);
    }

    this._strings = document.getElementById("updateStrings");
    this._installing = true;
    this.startNextInstall();
  },

  startNextInstall: function() {
    if (this._currentInstall >= 0) {
      this._installs[this._currentInstall].removeListener(this);
    }

    this._currentInstall++;

    if (this._installs.length == this._currentInstall) {
      this._installing = false;
      var nextPage = this._errors.length > 0 ? "installerrors" : "finished";
      document.getElementById("installing").setAttribute("next", nextPage);
      document.documentElement.advance();
      return;
    }

    this._installs[this._currentInstall].addListener(this);
    this._installs[this._currentInstall].install();
  },

  cancelInstalls: function() {
    this._installs[this._currentInstall].removeListener(this);
    this._installs[this._currentInstall].cancel();
  },

  /////////////////////////////////////////////////////////////////////////////
  // InstallListener
  onDownloadStarted: function(aInstall) {
    var strings = document.getElementById("updateStrings");
    var label = strings.getFormattedString("downloadingPrefix", [aInstall.name]);
    var actionItem = document.getElementById("actionItem");
    actionItem.value = label;
  },

  onDownloadProgress: function(aInstall) {
    var downloadProgress = document.getElementById("downloadProgress");
    downloadProgress.value = Math.ceil(100 * aInstall.progress / aInstall.maxProgress);
  },

  onDownloadEnded: function(aInstall) {
  },

  onDownloadFailed: function(aInstall) {
    this._errors.push(aInstall);

    this.startNextInstall();
  },

  onInstallStarted: function(aInstall) {
    var strings = document.getElementById("updateStrings");
    var label = strings.getFormattedString("installingPrefix", [aInstall.name]);
    var actionItem = document.getElementById("actionItem");
    actionItem.value = label;
  },

  onInstallEnded: function(aInstall, aAddon) {
    // Remember that this add-on was updated during startup
    AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED,
                                         aAddon.id);

    this.startNextInstall();
  },

  onInstallFailed: function(aInstall) {
    this._errors.push(aInstall);

    this.startNextInstall();
  }
};

var gInstallErrorsPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    document.documentElement.getButton("finish").focus();
  },
};

// Displayed when there are incompatible add-ons and the xpinstall.enabled
// pref is false and locked.
var gAdminDisabledPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, null, true,
                                  "cancelButtonText", true);
    document.documentElement.getButton("finish").focus();
  }
};

// Displayed when selected add-on updates have been installed without error.
// There can still be add-ons that are not compatible and don't have an update.
var gFinishedPage = {
  onPageShow: function ()
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
  onPageShow: function (aEvent)
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
