# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is The Update Service.
#
# The Initial Developer of the Original Code is Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com>
#   Robert Strong <robert.bugzilla@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

// This UI is only opened from the Extension Manager when the app is upgraded.

const nsIExtensionManager = Components.interfaces.nsIExtensionManager;
const nsIUpdateItem = Components.interfaces.nsIUpdateItem;
const nsIAUCL = Components.interfaces.nsIAddonUpdateCheckListener;

const PREF_UPDATE_EXTENSIONS_ENABLED            = "extensions.update.enabled";
const PREF_XPINSTALL_ENABLED                    = "xpinstall.enabled";

var gUpdateWizard = {
  // When synchronizing app compatibility info this contains all installed
  // add-ons. When checking for compatible versions this contains only
  // incompatible add-ons.
  items: [],
  // The items that we found updates available for
  itemsToUpdate: [],
  shouldSuggestAutoChecking: false,
  shouldAutoCheck: false,
  xpinstallEnabled: false,
  xpinstallLocked: false,

  init: function ()
  {
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                        .getService(nsIExtensionManager);
    // Retrieve all items in order to sync their app compatibility information
    this.items = em.getItemList(nsIUpdateItem.TYPE_ANY, { });
    var pref =
        Components.classes["@mozilla.org/preferences-service;1"].
        getService(Components.interfaces.nsIPrefBranch);

    try {
      this.shouldSuggestAutoChecking =
        !pref.getBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED);
    }
    catch (e) {
    }

    try {
      this.xpinstallEnabled = pref.getBoolPref(PREF_XPINSTALL_ENABLED);
      this.xpinstallLocked = pref.prefIsLocked(PREF_XPINSTALL_ENABLED);
    }
    catch (e) {
    }
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    if (ioService.offline)
      document.documentElement.currentPage =
        document.getElementById("offline");
    else
      document.documentElement.currentPage =
        document.getElementById("versioninfo");
  },

  onWizardFinish: function ()
  {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    if (this.shouldSuggestAutoChecking)
      pref.setBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED, this.shouldAutoCheck);
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
  showErrors: function (aState, aErrors)
  {
    openDialog("chrome://mozapps/content/extensions/errors.xul", "",
               "modal", { state: aState, errors: aErrors });
  },

  // Displays a list of items that had an error during the update check. We
  // don't display the actual error that occured since
  // nsIAddonUpdateCheckListener doesn't return the error details.
  showUpdateCheckErrors: function ()
  {
    var errors = [];
    for (var i = 0; i < this.errorItems.length; ++i)
      errors.push({ name: this.errorItems[i].name, error: true });
    this.showErrors("checking", errors);
  },

  checkForErrors: function (aElementIDToShow)
  {
    if (this.errorItems.length > 0)
      document.getElementById(aElementIDToShow).hidden = false;
  },

  onWizardClose: function (aEvent)
  {
    if (gInstallingPage._installing) {
      var os = Components.classes["@mozilla.org/observer-service;1"]
                         .getService(Components.interfaces.nsIObserverService);
      os.notifyObservers(null, "xpinstall-progress", "cancel");
      return false;
    }
    return true;
  }
};

var gOfflinePage = {
  onPageAdvanced: function ()
  {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    ioService.offline = false;
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
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(nsIExtensionManager);
    // Synchronize the app compatibility info for all items.
    em.update([], 0, nsIExtensionManager.UPDATE_SYNC_COMPATIBILITY, this);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIAddonUpdateCheckListener
  onUpdateStarted: function() {
    this._totalCount = gUpdateWizard.items.length;
  },

  onUpdateEnded: function() {
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(nsIExtensionManager);
    // Retrieve the remaining incompatible items.
    gUpdateWizard.items = em.getIncompatibleItemList(null, null, null,
                                                     nsIUpdateItem.TYPE_ANY,
                                                     true, { });
    if (gUpdateWizard.items.length > 0) {
      // There are still incompatible addons, inform the user.
      document.documentElement.currentPage =
        document.getElementById("mismatch");
    }
    else {
      // VersionInfo compatibility updates resolved all compatibility problems,
      // close this window and continue starting the application...
      //XXX Bug 314754 - We need to use setTimeout to close the window due to
      // the EM using xmlHttpRequest when checking for updates.
      setTimeout(close, 0);
    }
  },

  onAddonUpdateStarted: function(addon) {
  },

  onAddonUpdateEnded: function(addon, status) {
    if (status == nsIAUCL.STATUS_VERSIONINFO) {
      for (var i = 0; i < gUpdateWizard.items.length; ++i) {
        var item = gUpdateWizard.items[i].QueryInterface(nsIUpdateItem);
        if (addon.id == item.id) {
          gUpdateWizard.items.splice(i, 1);
          break;
        }
      }
    }
    else if (status == nsIAUCL.STATUS_FAILURE)
      gUpdateWizard.errorItems.push(addon);

    ++this._completeCount;

    // Update the status text and progress bar
    var updateStrings = document.getElementById("updateStrings");
    var status = document.getElementById("versioninfo.status");
    var statusString = updateStrings.getFormattedString("statusPrefix", [addon.name]);
    status.setAttribute("value", statusString);

    // Update the status text and progress bar
    var progress = document.getElementById("versioninfo.progress");
    progress.mode = "normal";
    progress.value = Math.ceil((this._completeCount / this._totalCount) * 100);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsISupports
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIAddonUpdateCheckListener) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

var gMismatchPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "mismatchCheckNow", false,
                                  "mismatchDontCheck", false);
    document.documentElement.getButton("next").focus();

    var incompatible = document.getElementById("mismatch.incompatible");
    for (var i = 0; i < gUpdateWizard.items.length; ++i) {
      var item = gUpdateWizard.items[i].QueryInterface(nsIUpdateItem);
      var listitem = document.createElement("listitem");
      listitem.setAttribute("label", item.name + " " + item.version);
      incompatible.appendChild(listitem);
    }
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

    this._totalCount = gUpdateWizard.items.length;
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(nsIExtensionManager);
    em.update(gUpdateWizard.items, this._totalCount,
              nsIExtensionManager.UPDATE_CHECK_NEWVERSION, this);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIAddonUpdateCheckListener
  onUpdateStarted: function() {
  },

  onUpdateEnded: function() {
    var nextPage = document.getElementById("noupdates");
    if (gUpdateWizard.itemsToUpdate.length > 0)
      nextPage = document.getElementById("found");
    document.documentElement.currentPage = nextPage;
  },

  onAddonUpdateStarted: function(addon) {
  },

  onAddonUpdateEnded: function(addon, status) {
    if (status == nsIAUCL.STATUS_UPDATE)
      gUpdateWizard.itemsToUpdate.push(addon);
    else if (status == nsIAUCL.STATUS_FAILURE)
      gUpdateWizard.errorItems.push(addon);

    ++this._completeCount;

    // Update the status text and progress bar
    var updateStrings = document.getElementById("updateStrings");
    var status = document.getElementById("checking.status");
    var statusString = updateStrings.getFormattedString("statusPrefix", [addon.name]);
    status.setAttribute("value", statusString);

    var progress = document.getElementById("checking.progress");
    progress.value = Math.ceil((this._completeCount / this._totalCount) * 100);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsISupports
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIAddonUpdateCheckListener) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

var gFoundPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "installButtonText", false,
                                  null, false);

    var foundUpdates = document.getElementById("found.updates");
    var itemCount = gUpdateWizard.itemsToUpdate.length;
    for (var i = 0; i < itemCount; ++i) {
      var item = gUpdateWizard.itemsToUpdate[i];
      var listItem = foundUpdates.appendItem(item.name + " " + item.version,
                                             item.xpiURL);
      listItem.setAttribute("type", "checkbox");
      listItem.setAttribute("checked", "true");
      listItem.setAttribute("URL", item.xpiURL);
      listItem.setAttribute("hash", item.xpiHash);
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
    var updates = foundUpdates.getElementsByTagName("listitem");;
    for (var i = 0; i < updates.length; ++i) {
      if (!updates[i].checked)
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
  _installing       : false,
  _objs             : [],
  _errors           : [],

  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true,
                                  "nextButtonText", true,
                                  null, true);

    // Get XPInstallManager and kick off download/install
    // process, registering us as an observer.
    var items = [];
    var hashes = [];
    this._objs = [];
    this._errors = [];

    var foundUpdates = document.getElementById("found.updates");
    var updates = foundUpdates.getElementsByTagName("listitem");;
    for (var i = 0; i < updates.length; ++i) {
      if (!updates[i].checked)
        continue;
      items.push(updates[i].value);
      hashes.push(updates[i].getAttribute("hash") ? updates[i].getAttribute("hash") : null);
      this._objs.push({ name: updates[i].label });
    }

    var xpimgr = Components.classes["@mozilla.org/xpinstall/install-manager;1"]
                           .createInstance(Components.interfaces.nsIXPInstallManager);
    xpimgr.initManagerWithHashes(items, hashes, items.length, this);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIXPIProgressDialog
  onStateChange: function (aIndex, aState, aValue)
  {
    var strings = document.getElementById("updateStrings");

    const nsIXPIProgressDialog = Components.interfaces.nsIXPIProgressDialog;
    switch (aState) {
    case nsIXPIProgressDialog.DOWNLOAD_START:
      var label = strings.getFormattedString("downloadingPrefix", [this._objs[aIndex].name]);
      var actionItem = document.getElementById("actionItem");
      actionItem.value = label;
      break;
    case nsIXPIProgressDialog.DOWNLOAD_DONE:
    case nsIXPIProgressDialog.INSTALL_START:
      var label = strings.getFormattedString("installingPrefix", [this._objs[aIndex].name]);
      var actionItem = document.getElementById("actionItem");
      actionItem.value = label;
      this._installing = true;
      break;
    case nsIXPIProgressDialog.INSTALL_DONE:
      switch (aValue) {
      case 999:
      case 0:
        break;
      default:
        this._errors.push({ name: this._objs[aIndex].name, error: aValue });
      }
      break;
    case nsIXPIProgressDialog.DIALOG_CLOSE:
      this._installing = false;
      var nextPage = this._errors.length > 0 ? "installerrors" : "finished";
      document.getElementById("installing").setAttribute("next", nextPage);
      document.documentElement.advance();
      break;
    }
  },

  onProgress: function (aIndex, aValue, aMaxValue)
  {
    var downloadProgress = document.getElementById("downloadProgress");
    downloadProgress.value = Math.ceil((aValue/aMaxValue) * 100);
  }
};

var gInstallErrorsPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    document.documentElement.getButton("finish").focus();
  },

  onShowErrors: function ()
  {
    gUpdateWizard.showErrors("install", gInstallingPage._errors);
  }
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
