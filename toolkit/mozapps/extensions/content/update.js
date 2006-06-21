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

//
// This UI can be opened from the following places, and in the following modes:
//
// - from the Version Compatibility Checker at startup
//    as the application starts a check is done to see if the application being
//    started is the same version that was started last time. If it isn't, a
//    list of UpdateItems that are incompatible with the verison being 
//    started is generated and this UI opened with that list. This UI then
//    offers to check for updates to those UpdateItems that are compatible
//    with the version of the application being started. 
//    
//    In this mode, the wizard is opened to panel 'mismatch'. 

const nsIUpdateItem = Components.interfaces.nsIUpdateItem;
const nsIAUCL = Components.interfaces.nsIAddonUpdateCheckListener;

const PREF_UPDATE_EXTENSIONS_ENABLED            = "extensions.update.enabled";
const PREF_UPDATE_EXTENSIONS_AUTOUPDATEENABLED  = "extensions.update.autoUpdateEnabled";
const PREF_XPINSTALL_ENABLED                    = "xpinstall.enabled";

var gShowMismatch = null;

var gUpdateWizard = {
  // The items to check for updates for (e.g. an extension, some subset of extensions, 
  // all extensions, a list of compatible extensions, etc...)
  items: [],
  // The items that we found updates available for
  itemsToUpdate: [],
  // The items that we successfully installed updates for
  updatedCount: 0,
  shouldSuggestAutoChecking: false,
  shouldAutoCheck: false,
  xpinstallEnabled: false,
  xpinstallLocked: false,
  remainingExtensionUpdateCount: 0,
  
  init: function ()
  {
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                        .getService(Components.interfaces.nsIExtensionManager);
    // Retrieve all items in order to sync their app compatibility information
    this.items = em.getItemList(nsIUpdateItem.TYPE_ADDON, { });
    var pref = 
        Components.classes["@mozilla.org/preferences-service;1"].
        getService(Components.interfaces.nsIPrefBranch);
    this.shouldSuggestAutoChecking = 
      gShowMismatch && 
      !pref.getBoolPref(PREF_UPDATE_EXTENSIONS_AUTOUPDATEENABLED);

    this.xpinstallEnabled = pref.getBoolPref(PREF_XPINSTALL_ENABLED);
    this.xpinstallLocked = pref.prefIsLocked(PREF_XPINSTALL_ENABLED);
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
    openDialog("chrome://mozapps/content/update/errors.xul", "", 
               "modal", { state: aState, errors: aErrors });
  },
  
  showUpdateCheckErrors: function ()
  {
    var errors = [];
    for (var i = 0; i < this.errorItems.length; ++i)
      errors.push({ name: this.errorItems[i].name, error: true, 
                    item: this.errorItems[i] });
    this.showErrors("checking", errors);
  },

  checkForErrors: function (aElementIDToShow)
  {
    if (this.errorOnGeneric || this.errorItems.length > 0)
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

var gVersionInfoPage = {
  _completeCount: 0,
  _totalCount: 0,
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, 
                                  "nextButtonText", true, 
                                  "cancelButtonText", false);
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    // Synchronize the app compatibility info for all items by specifying 2 for
    // the versionUpdateOnly parameter.
    em.update([], 0, 2, this);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIAddonUpdateCheckListener
  onUpdateStarted: function() {
    this._totalCount = gUpdateWizard.items.length;
  },
  
  onUpdateEnded: function() {
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    // Retrieve the remaining incompatible items.
    gUpdateWizard.items = em.getIncompatibleItemList(null, null,
                                                     nsIUpdateItem.TYPE_ADDON,
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
    ++this._completeCount;

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
  _completeCount: 0,
  onPageShow: function ()
  {
    if (!gUpdateWizard.xpinstallEnabled && gUpdateWizard.xpinstallLocked) {
      document.documentElement.currentPage = document.getElementById("finished");
      return;
    }

    gUpdateWizard.setButtonLabels(null, true, 
                                  "nextButtonText", true, 
                                  "cancelButtonText", false);
    document.documentElement.getButton("next").focus();

    gUpdateWizard.errorItems = [];
    
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    em.update(gUpdateWizard.items, gUpdateWizard.items.length, false, this);
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
    else if (status == nsIAUCL.STATE_ERROR)
      gUpdateWizard.errorItems.push(addon);
      
    ++this._completeCount;

    // Update the status text and progress bar
    var updateStrings = document.getElementById("updateStrings");
    var status = document.getElementById("checking.status");
    var statusString = updateStrings.getFormattedString("checkingPrefix", [addon.name]);
    status.setAttribute("value", statusString);

    var progress = document.getElementById("checking.progress");
    progress.value = Math.ceil((this._completeCount / gUpdateWizard.items.length) * 100);
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
  _nonAppItems: [],
  
  _newestInfo: null,

  buildAddons: function ()
  {
    var hasExtensions = false;
    var foundAddonsList = document.getElementById("found.addons.list");
    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);
    var itemCount = gUpdateWizard.itemsToUpdate.length;
    for (var i = 0; i < itemCount; ++i) {
      var item = gUpdateWizard.itemsToUpdate[i];
      var checkbox = document.createElement("checkbox");
      foundAddonsList.appendChild(checkbox);
      checkbox.setAttribute("type", "update");
      checkbox.label        = item.name + " " + item.version;
      checkbox.setAttribute("URL", item.xpiURL);
      checkbox.setAttribute("hash", item.xpiHash);
      checkbox.infoURL      = "";
      checkbox.internalName = "";
      uri.spec              = item.xpiURL;
      checkbox.setAttribute("source", uri.host);
      checkbox.checked      = true;
      hasExtensions         = true;
    }

    if (hasExtensions) {
      var addonsHeader = document.getElementById("addons");
      var strings = document.getElementById("updateStrings");
      addonsHeader.label = strings.getFormattedString("updateTypeAddons", [itemCount]);
      addonsHeader.collapsed = false;
    }
  },

  _initialized: false,
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, 
                                  "installButtonText", false, 
                                  null, false);
    if (!gUpdateWizard.xpinstallEnabled) {
      document.documentElement.getButton("next").disabled = true;
      document.getElementById("xpinstallDisabledAlert").hidden = false;
      document.getElementById("enableXPInstall").focus();
    }
    else
      document.documentElement.getButton("next").focus();
    
    var updates = document.getElementById("found.updates");
    if (!this._initialized) {
      this._initialized = true;
      updates.computeSizes();
      this.buildAddons();
    }
        
    var kids = updates._getRadioChildren();
    for (var i = 0; i < kids.length; ++i) {
      if (kids[i].collapsed == false) {
        updates.selectedIndex = i;
        break;
      }
    }
  },
    
  toggleXPInstallEnable: function(aEvent)
  {
    var enabled = aEvent.target.checked;
    gUpdateWizard.xpinstallEnabled = enabled;
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    pref.setBoolPref(PREF_XPINSTALL_ENABLED, enabled);
    document.documentElement.getButton("next").disabled = !enabled;
  },

  onSelect: function (aEvent)
  {
    if (!gUpdateWizard.xpinstallEnabled)
      return;

    var updates = document.getElementById("found.updates");
    var oneChecked = false;
    var items = updates.selectedItem.getElementsByTagName("checkbox");
    for (var i = 0; i < items.length; ++i) {  
      if (items[i].checked) {
        oneChecked = true;
        break;
      }
    }

    var strings = document.getElementById("updateStrings");
    gUpdateWizard.setButtonLabels(null, true, 
                                  "installButtonText", true, 
                                  null, false);
    var text = strings.getString("foundInstructions");
    document.getElementById("found").setAttribute("next", "installing"); 
    document.documentElement.getButton("next").disabled = !oneChecked;

    var foundInstructions = document.getElementById("foundInstructions");
    while (foundInstructions.hasChildNodes())
      foundInstructions.removeChild(foundInstructions.firstChild);
    foundInstructions.appendChild(document.createTextNode(text));
  }
};

var gInstallingPage = {
  _installing       : false,
  _restartRequired  : false,
  _objs             : [],
  
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
    
    this._restartRequired = false;
    
    gUpdateWizard.remainingExtensionUpdateCount = gUpdateWizard.itemsToUpdate.length;

    var updates = document.getElementById("found.updates");
    var checkboxes = updates.selectedItem.getElementsByTagName("checkbox");
    for (var i = 0; i < checkboxes.length; ++i) {
      if (checkboxes[i].type == "update" && checkboxes[i].checked) {
        items.push(checkboxes[i].URL);
        hashes.push(checkboxes[i].hash ? checkboxes[i].hash : null);
        this._objs.push({ name: checkboxes[i].label });
      }
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
        this._restartRequired = true;
        break;
      case 0: 
        --gUpdateWizard.remainingExtensionUpdateCount;
        break;
      }
      break;
    case nsIXPIProgressDialog.DIALOG_CLOSE:
      this._installing = false;
      var nextPage = this._errors ? "errors" : (this._restartRequired ? "restart" : "finished");
      document.getElementById("installing").setAttribute("next", nextPage);
      document.documentElement.advance();
      break;
    }
  },
  
  _objs: [],
  _errors: false,
  
  onProgress: function (aIndex, aValue, aMaxValue)
  {
    var downloadProgress = document.getElementById("downloadProgress");
    downloadProgress.value = Math.ceil((aValue/aMaxValue) * 100);
  }
};

var gErrorsPage = {
  onPageShow: function ()
  {
    document.documentElement.getButton("finish").focus();
  },
  
  onShowErrors: function ()
  {
    gUpdateWizard.showErrors("install", gInstallingPage._objs);
  }  
};

var gFinishedPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    document.documentElement.getButton("finish").focus();
    
    if (gUpdateWizard.xpinstallLocked)
    {
      document.getElementById("adminDisabled").hidden = false;
      document.getElementById("incompatibleRemainingLocked").hidden = false;
    }
    else {
      document.getElementById("updated").hidden = false;
      document.getElementById("incompatibleRemaining").hidden = false;
      if (gUpdateWizard.shouldSuggestAutoChecking) {
        document.getElementById("finishedEnableChecking").hidden = false;
        document.getElementById("finishedEnableChecking").click();
      }
    }
    
    if (gShowMismatch || gUpdateWizard.xpinstallLocked) {
      document.getElementById("finishedMismatch").hidden = false;
      document.getElementById("incompatibleAlert").hidden = false;
    }
  }
};

var gNoUpdatesPage = {
  onPageShow: function (aEvent)
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    document.documentElement.getButton("finish").focus();
    if (gShowMismatch) {
      document.getElementById("introUser").hidden = true;
      document.getElementById("introMismatch").hidden = false;
      document.getElementById("mismatchNoUpdates").hidden = false;
        
      if (gUpdateWizard.shouldSuggestAutoChecking) {
        document.getElementById("mismatchIncompatibleRemaining").hidden = true;
        document.getElementById("mismatchIncompatibleRemaining2").hidden = false;
        document.getElementById("mismatchFinishedEnableChecking").hidden = false;
      }
    }

    gUpdateWizard.checkForErrors("updateCheckErrorNotFound");
  }
};

