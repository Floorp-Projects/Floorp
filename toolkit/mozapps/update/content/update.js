//
// window.arguments[1...] is an array of nsIUpdateItem implementing objects 
// that are to be updated. 
//  * if the array is empty, all items are updated (like a background update
//    check)
//  * if the array contains one or two UpdateItems, with null id fields, 
//    all items of that /type/ are updated.
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
//
// - from the Extension Manager or Options Dialog or any UI where the user
//   directly requests to check for updates.
//    in this case the user selects UpdateItem(s) to update and this list
//    is passed to this UI. If a single item is sent with the id field set to
//    null but the type set correctly, this UI will check for updates to all 
//    items of that type.
//
//    In this mode, the wizard is opened to panel 'checking'.
//
// - from the Updates Available Status Bar notification.
//    in this case the background update checking has determined there are new
//    updates that can be installed and has prepared a list for the user to see.
//    They can update by clicking on a status bar icon which passes the list
//    to this UI which lets them choose to install updates. 
//
//    In this mode, the wizard is opened to panel 'updatesFound' if the data
//    set is immediately available, or 'checking' if the user closed the browser
//    since the last background check was performed and the check needs to be
//    performed again. 
//

const nsIUpdateItem                     = Components.interfaces.nsIUpdateItem;
const nsIUpdateService                  = Components.interfaces.nsIUpdateService;
const nsIExtensionManager               = Components.interfaces.nsIExtensionManager;

const PREF_APP_ID                           = "app.id";
const PREF_UPDATE_EXTENSIONS_ENABLED        = "update.extensions.enabled";
const PREF_UPDATE_APP_UPDATESAVAILABLE      = "update.app.updatesAvailable";
const PREF_UPDATE_APP_UPDATEVERSION         = "update.app.updateVersion";
const PREF_UPDATE_APP_UPDATEDESCRIPTION     = "update.app.updateDescription";
const PREF_UPDATE_APP_UPDATEURL             = "update.app.updateURL";
const PREF_UPDATE_APP_PERFORMED             = "update.app.performed";

const PREF_UPDATE_EXTENSIONS_COUNT          = "update.extensions.count";
const PREF_UPDATE_EXTENSIONS_SEVERITY_THRESHOLD = "update.extensions.severity.threshold";
const PREF_UPDATE_SEVERITY                  = "update.severity";

var gSourceEvent = null;
var gUpdateTypes = null;

var gUpdateWizard = {
  // The items to check for updates for (e.g. an extension, some subset of extensions, 
  // all extensions, a list of compatible extensions, etc...)
  items: [],
  // The items that we found updates available for
  itemsToUpdate: [],
  // The items that we successfully installed updates for
  updatedCount: 0,
  appUpdatesAvailable: false,

  shouldSuggestAutoChecking: false,
  shouldAutoCheck: false,
  
  updatingApp: false,
  remainingExtensionUpdateCount: 0,

  appComps: {
    upgraded: { 
      core      : [],
      optional  : [],
      languages : [],
    },
    optional: {
      optional  : [],
      languages : [],
    }
  },
  selectedLocaleAvailable: false,

  init: function ()
  {
    gUpdateTypes = window.arguments[0];
    gSourceEvent = window.arguments[1];
    for (var i = 2; i < window.arguments.length; ++i)
      this.items.push(window.arguments[i].QueryInterface(nsIUpdateItem));

    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    this.shouldSuggestAutoChecking = (gSourceEvent == nsIUpdateService.SOURCE_EVENT_MISMATCH) && 
                                      !pref.getBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED);

    if (gSourceEvent == nsIUpdateService.SOURCE_EVENT_MISMATCH) {
      var version = document.getElementById("version")
      version.setAttribute("next", "mismatch");
    }
    
    gMismatchPage.init();
  },
  
  uninit: function ()
  {
    // Ensure all observers are unhooked, just in case something goes wrong or the
    // user aborts. 
    gVersionPage.uninit();
    gUpdatePage.uninit();  
  },
  
  onWizardFinish: function ()
  {
    if (this.shouldSuggestAutoChecking) {
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      pref.setBoolPref("update.extensions.enabled", this.shouldAutoCheck); 
    }
    
    if (this.updatingApp) {
      // Clear the "app update available" pref as an interim amnesty assuming
      // the user actually does install the new version. If they don't, a subsequent
      // update check will poke them again.
      this.clearAppUpdatePrefs();
    }
    else {
      // Downloading and Installed Extension
      this.clearExtensionUpdatePrefs();
    }

    // Send an event to refresh any FE notification components. 
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    var userEvt = Components.interfaces.nsIUpdateService.SOURCE_EVENT_USER;
    os.notifyObservers(null, "Update:Ended", userEvt.toString());
  },
  
  clearAppUpdatePrefs: function ()
  {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
                         
    // Set the "applied app updates this session" pref so that the update service
    // does not display the update notifier for application updates anymore this
    // session, to give the user a one-cycle amnesty to install the newer version.
    var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                            .getService(Components.interfaces.nsIUpdateService);
    updates.appUpdatesAvailable = false;
    
    pref.setBoolPref(PREF_UPDATE_APP_PERFORMED, true);    

    // Unset prefs used by the update service to signify application updates
    if (pref.prefHasUserValue(PREF_UPDATE_APP_UPDATESAVAILABLE))
      pref.clearUserPref(PREF_UPDATE_APP_UPDATESAVAILABLE);
    if (pref.prefHasUserValue(PREF_UPDATE_APP_UPDATEVERSION))
      pref.clearUserPref(PREF_UPDATE_APP_UPDATEVERSION);
    if (pref.prefHasUserValue(PREF_UPDATE_APP_UPDATEURL))
      pref.clearUserPref(PREF_UPDATE_APP_UPDATEDESCRIPTION);
    if (pref.prefHasUserValue(PREF_UPDATE_APP_UPDATEURL)) 
      pref.clearUserPref(PREF_UPDATE_APP_UPDATEURL);

    // Lower the severity to reflect the fact that there are now only Extension/
    // Theme updates available
    var newCount = pref.getIntPref(PREF_UPDATE_EXTENSIONS_COUNT);
    var threshold = pref.getIntPref(PREF_UPDATE_EXTENSIONS_SEVERITY_THRESHOLD);
    if (newCount >= threshold)
      pref.setIntPref(PREF_UPDATE_SEVERITY, nsIUpdateService.SEVERITY_MEDIUM);
    else
      pref.setIntPref(PREF_UPDATE_SEVERITY, nsIUpdateService.SEVERITY_LOW);
  },
  
  clearExtensionUpdatePrefs: function ()
  {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
                         
    // Set the "applied extension updates this session" pref so that the 
    // update service does not display the update notifier for extension
    // updates anymore this session (the updates will be applied at the next
    // start).
    var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                            .getService(Components.interfaces.nsIUpdateService);
    updates.extensionUpdatesAvailable = this.remainingExtensionUpdateCount;
    
    if (pref.prefHasUserValue(PREF_UPDATE_EXTENSIONS_COUNT)) 
      pref.clearUserPref(PREF_UPDATE_EXTENSIONS_COUNT);
  },
  
  _setUpButton: function (aButtonID, aButtonKey, aDisabled)
  {
    var strings = document.getElementById("updateStrings");
    var button = document.documentElement.getButton(aButtonID);
    if (aButtonKey) {
      button.label = strings.getString(aButtonKey);
      try {
        button.accesskey = strings.getString(aButtonKey + "Accesskey");
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
  errorOnApp: false,

  showErrors: function (aState, aErrors)
  {
    openDialog("chrome://mozapps/content/update/errors.xul", "", 
               "modal", { state: aState, errors: aErrors });
  },
  
  showUpdateCheckErrors: function ()
  {
    var errors = [];
    for (var i = 0; i < this.errorItems.length; ++i)
      errors.push({ name: this.errorItems[i].name, error: true });

    if (this.errorOnApp) {
      var brandShortName = document.getElementById("brandStrings").getString("brandShortName");
      errors.push({ name: brandShortName, error: true });    
    }
    
    this.showErrors("checking", errors);
  },

  checkForErrors: function (aElementIDToShow)
  {
    if (this.errorOnGeneric || this.errorItems.length > 0 || this.errorOnApp)
      document.getElementById(aElementIDToShow).hidden = false;
  },
  
  onWizardClose: function (aEvent)
  {
    if (gInstallingPage._installing)
      return false;
    return true;
  }
};

var gVersionPage = {
  _completeCount: 0,
  _messages: ["Version:Extension:Started", 
              "Version:Extension:Ended", 
              "Version:Extension:Item-Started", 
              "Version:Extension:Item-Ended",
              "Version:Extension:Item-Error"],
  
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, 
                                  "nextButtonText", true, 
                                  "cancelButtonText", true);
    document.documentElement.getButton("next").focus();
    
    // If the user has disabled Extension/Theme update checking, don't bother
    // doing version compatibility updates first. 
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    if (!pref.getBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED))
      document.documentElement.advance();

    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    for (var i = 0; i < this._messages.length; ++i)
      os.addObserver(this, this._messages[i], false);

    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    em.update(gUpdateWizard.items, gUpdateWizard.items.length, 
              nsIExtensionManager.UPDATE_MODE_VERSION, true);
  },

  _destroyed: false,  
  uninit: function ()
  {
    if (this._destroyed)
      return;
  
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    for (var i = 0; i < this._messages.length; ++i)
      os.removeObserver(this, this._messages[i]);

    this._destroyed = true;
  },

  _totalCount: 0,
  get totalCount()
  {
    if (!this._totalCount) {
      this._totalCount = gUpdateWizard.items.length;
      if (this._totalCount == 0) {
        var em = Components.classes["@mozilla.org/extensions/manager;1"]
                            .getService(Components.interfaces.nsIExtensionManager);
        var extensionCount = em.getItemList(null, nsIUpdateItem.TYPE_EXTENSION, {}).length;
        var themeCount = em.getItemList(null, nsIUpdateItem.TYPE_THEME, {}).length;

        this._totalCount = extensionCount + themeCount;
      }
    }
    return this._totalCount;
  },  
  
  observe: function (aSubject, aTopic, aData)
  {
    var canFinish = false;
    switch (aTopic) {
    case "Version:Extension:Started":
      break;
    case "Version:Extension:Item-Started":
      break;
    case "Version:Extension:Item-Ended":
      if (aSubject) {
        var item = aSubject.QueryInterface(Components.interfaces.nsIUpdateItem);
        var updateStrings = document.getElementById("updateStrings");
        var status = document.getElementById("checking.status");
        var statusString = updateStrings.getFormattedString("checkingPrefix", [item.name]);
        status.setAttribute("value", statusString);
        
        if (gSourceEvent == nsIUpdateService.SOURCE_EVENT_MISMATCH) {
          // In mismatch mode it is sufficient to perform a version update
          // on Extensions and Themes - a proper update check can occur later.
          var ary = [];
          for (var i = 0; i < gUpdateWizard.items.length; ++i) {
            if (gUpdateWizard.items[i].id != item.id)
              ary.push(gUpdateWizard.items[i]);
          }
          gUpdateWizard.items = ary;
        }
      }
      ++this._completeCount;

      // Update the Progress Bar            
      var progress = document.getElementById("checking.progress");
      progress.value = Math.ceil((this._completeCount / this.totalCount) * 100);
      
      break;
    case "Version:Extension:Item-Error":
      ++this._completeCount;
      var progress = document.getElementById("checking.progress");
      progress.value = Math.ceil((this._completeCount / this.totalCount) * 100);

      break;
    case "Version:Extension:Ended":
      gVersionPage.uninit();
      
      if (gUpdateWizard.items.length == 0 && 
          gSourceEvent == nsIUpdateService.SOURCE_EVENT_MISMATCH) {
        // We've resolved all compatibilities in this Version Update, so
        // close up.
        var updateStrings = document.getElementById("updateStrings");
        var status = document.getElementById("checking.status");
        status.setAttribute("value", updateStrings.getString("versionUpdateComplete"));

        var closeTimer = Components.classes["@mozilla.org/timer;1"]
                                   .createInstance(Components.interfaces.nsITimer);
        closeTimer.initWithCallback(this, 2000, 
                                    Components.interfaces.nsITimer.TYPE_ONE_SHOT);
        break;
      }
      document.documentElement.advance();
      break;
    }
  },
  
  notify: function (aTimer)
  {
    window.close();
  }
};

var gMismatchPage = {
  init: function ()
  {
    var incompatible = document.getElementById("mismatch.incompatible");
    for (var i = 0; i < gUpdateWizard.items.length; ++i) {
      var item = gUpdateWizard.items[i];
      var listitem = document.createElement("listitem");
      listitem.setAttribute("label", item.name + " " + item.version);
      incompatible.appendChild(listitem);
    }
  },
  
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, 
                                  "mismatchCheckNow", false, 
                                  "mismatchDontCheck", false);
    document.documentElement.getButton("next").focus();
  }
};

var gUpdatePage = {
  _completeCount: 0,
  _messages: ["Update:Extension:Started", 
              "Update:Extension:Ended", 
              "Update:Extension:Item-Started", 
              "Update:Extension:Item-Ended",
              "Update:Extension:Item-Error",
              "Update:App:Ended",
              "Update:Ended"],
  
  onPageShow: function ()
  {
    if (!gUpdateTypes)
      gUpdateWizard.init();
      
    gUpdateWizard.setButtonLabels(null, true, 
                                  "nextButtonText", true, 
                                  "cancelButtonText", true);
    document.documentElement.getButton("next").focus();

    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    for (var i = 0; i < this._messages.length; ++i)
      os.addObserver(this, this._messages[i], false);

    gUpdateWizard.errorItems = [];

    var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                            .getService(Components.interfaces.nsIUpdateService);
    updates.checkForUpdatesInternal(gUpdateWizard.items, gUpdateWizard.items.length, 
                                    gUpdateTypes, gSourceEvent);
  },

  _destroyed: false,  
  uninit: function ()
  {
    if (this._destroyed)
      return;
  
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    for (var i = 0; i < this._messages.length; ++i)
      os.removeObserver(this, this._messages[i]);

    this._destroyed = true;
  },

  _totalCount: 0,
  get totalCount()
  {
    if (!this._totalCount) {
      this._totalCount = gUpdateWizard.items.length;
      if (this._totalCount == 0) {
        var em = Components.classes["@mozilla.org/extensions/manager;1"]
                            .getService(Components.interfaces.nsIExtensionManager);
        var extensionCount = em.getItemList(null, nsIUpdateItem.TYPE_EXTENSION, {}).length;
        var themeCount = em.getItemList(null, nsIUpdateItem.TYPE_THEME, {}).length;

        this._totalCount = extensionCount + themeCount + 1;
      }
    }
    return this._totalCount;
  },  
  
  observe: function (aSubject, aTopic, aData)
  {
    var canFinish = false;
    switch (aTopic) {
    case "Update:Extension:Started":
      break;
    case "Update:Extension:Item-Started":
      break;
    case "Update:Extension:Item-Ended":
      if (aSubject) {
        var item = aSubject.QueryInterface(Components.interfaces.nsIUpdateItem);
        gUpdateWizard.itemsToUpdate.push(item);
      
        var updateStrings = document.getElementById("updateStrings");
        var status = document.getElementById("checking.status");
        var statusString = updateStrings.getFormattedString("checkingPrefix", [item.name]);
        status.setAttribute("value", statusString);
      }
      ++this._completeCount;

      // Update the Progress Bar            
      var progress = document.getElementById("checking.progress");
      progress.value = Math.ceil((this._completeCount / this.totalCount) * 100);
      
      break;
    case "Update:Extension:Item-Error":
      if (aSubject) {
        var item = aSubject.QueryInterface(Components.interfaces.nsIUpdateItem);
        gUpdateWizard.errorItems.push(item);
      }
      else {
        for (var i = 0; i < gUpdateWizard.items.length; ++i) {
          if (!gUpdateWizard.items[i].updateRDF)
            gUpdateWizard.errorItems.push(gUpdateWizard.items[i]);
        }
      }
      ++this._completeCount;
      var progress = document.getElementById("checking.progress");
      progress.value = Math.ceil((this._completeCount / this.totalCount) * 100);

      break;
    case "Update:Extension:Ended":
      // If we were passed a set of extensions/themes/other to update, this
      // means we're not checking for app updates, so don't wait for the app
      // update to complete before advancing (because there is none).
      // canFinish = gUpdateWizard.items.length > 0;
      // XXXben
      break;
    case "Update:Ended":
      // If we're doing a general update check, (that is, no specific extensions/themes
      // were passed in for us to check for updates to), this notification means both
      // extension and app updates have completed.
      canFinish = true;
      break;
    case "Update:App:Error":
      gUpdateWizard.errorOnApp = true;
      ++this._completeCount;
      var progress = document.getElementById("checking.progress");
      progress.value = Math.ceil((this._completeCount / this.totalCount) * 100);
      break;
    case "Update:App:Ended":
      // The "Updates Found" page of the update wizard needs to know if it there are app 
      // updates so it can list them first. 
      ++this._completeCount;
      var progress = document.getElementById("checking.progress");
      progress.value = Math.ceil((this._completeCount / this.totalCount) * 100);
      break;
    }

    if (canFinish) {
      gUpdatePage.uninit();
      var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                              .getService(Components.interfaces.nsIUpdateService);
      if (gUpdateWizard.itemsToUpdate.length > 0 || updates.appUpdatesAvailable)
        document.getElementById("checking").setAttribute("next", "found");
      document.documentElement.advance();
    }
  }
};

var gFoundPage = {
  _appUpdateExists: false,
  _appSelected: false, 
  _appItem: null,
  _nonAppItems: [],
  
  _newestInfo: null,
  _currentInfo: null,
  
  buildAddons: function ()
  {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    if (!pref.getBoolPref(PREF_UPDATE_EXTENSIONS_ENABLED))
      return;

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
      checkbox.URL          = item.xpiURL;
      checkbox.infoURL      = "";
      checkbox.internalName = "";
      uri.spec              = item.xpiURL;
      checkbox.source       = uri.host;
      checkbox.checked      = true;
      hasExtensions         = true;
    }

    if (hasExtensions) {
      var addonsHeader = document.getElementById("addons");
      var strings = document.getElementById("updateStrings");
      addonsHeader.label = strings.getFormattedString("updateTypeExtensions", [itemCount]);
      addonsHeader.collapsed = false;
    }
  },

  buildPatches: function (aPatches)
  {
    var needsPatching = false;
    var critical = document.getElementById("found.criticalUpdates.list");
    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);
    var count = 0;
    for (var i = 0; i < aPatches.length; ++i) {
      var ver = InstallTrigger.getVersion(aPatches[i].internalName);
      if (InstallTrigger.getVersion(aPatches[i].internalName)) {
        // The user has already installed this patch since info
        // about it exists in the Version Registry. Skip. 
        continue;
      }
      
      var checkbox = document.createElement("checkbox");
      critical.appendChild(checkbox);
      checkbox.setAttribute("type", "update");
      checkbox.label        = aPatches[i].name;
      checkbox.URL          = aPatches[i].URL;
      checkbox.infoURL      = aPatches[i].infoURL;
      checkbox.internalName = aPatches[i].internalName;
      uri.spec              = checkbox.URL;
      checkbox.source       = uri.host;
      checkbox.checked      = true;
      needsPatching         = true;
      ++count;
    }
    
    if (needsPatching) {
      var patchesHeader = document.getElementById("patches");
      var strings = document.getElementById("updateStrings");
      patchesHeader.label = strings.getFormattedString("updateTypePatches", [count]);
      patchesHeader.collapsed = false;
    }
  },
  
  buildApp: function (aFiles)
  {
    // A New version of the app is available. 
    var app = document.getElementById("app");
    var strings = document.getElementById("updateStrings");
    var brandStrings = document.getElementById("brandStrings");
    var brandShortName = brandStrings.getString("brandShortName");
    app.label = strings.getFormattedString("appNameAndVersionFormat", 
                                            [brandShortName, 
                                             this._newestInfo.updateDisplayVersion]);
    app.accesskey = brandShortName.charAt(0);
    app.collapsed = false;

    var foundAppLabel = document.getElementById("found.app.label");
    var text = strings.getFormattedString("foundAppLabel",
                                          [brandShortName, 
                                           this._newestInfo.updateDisplayVersion])
    foundAppLabel.appendChild(document.createTextNode(text));

    var features = this._newestInfo.getCollection("features", { });
    if (features) {
      var foundAppFeatures = document.getElementById("found.app.features");
      foundAppFeatures.hidden = false;
      text = strings.getFormattedString("foundAppFeatures", 
                                        [brandShortName, 
                                         this._newestInfo.updateDisplayVersion]);
      foundAppFeatures.appendChild(document.createTextNode(text));

      var foundAppFeaturesList = document.getElementById("found.app.featuresList");
      for (var i = 0; i < features.length; ++i) {
        var feature = document.createElement("label");
        foundAppFeaturesList.appendChild(feature);
        feature.setAttribute("value", features[i].name);
      }
    }
    
    var foundAppInfoLink = document.getElementById("found.app.infoLink");
    foundAppInfoLink.href = this._newestInfo.updateInfoURL;
  },
  
  buildOptional: function (aComponents)
  {
    var needsOptional = false;
    var critical = document.getElementById("found.components.list");
    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);
    var count = 0;
    for (var i = 0; i < aComponents.length; ++i) {
      if (InstallTrigger.getVersion(aComponents[i].internalName)) {
        // The user has already installed this patch since info
        // about it exists in the Version Registry. Skip. 
        continue;
      }
      
      var checkbox = document.createElement("checkbox");
      critical.appendChild(checkbox);
      checkbox.setAttribute("type", "update");
      checkbox.label        = aComponents[i].name;
      checkbox.URL          = aComponents[i].URL;
      checkbox.infoURL      = aComponents[i].infoURL;
      checkbox.internalName = aComponents[i].internalName;
      uri.spec              = checkbox.URL;
      checkbox.source       = uri.host;
      needsOptional         = true;
      ++count;
    }
    
    if (needsOptional) {
      var optionalHeader = document.getElementById("components");
      var strings = document.getElementById("updateStrings");
      optionalHeader.label = strings.getFormattedString("updateTypeComponents", [count]);
      optionalHeader.collapsed = false;
    }
  },

  buildLanguages: function (aLanguages)
  {
    var hasLanguages = false;
    var languageList = document.getElementById("found.languages.list");
    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);
    var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                       .getService(Components.interfaces.nsIXULChromeRegistry);
    var selectedLocale = cr.getSelectedLocale("global");
    var count = 0;
    for (var i = 0; i < aLanguages.length; ++i) {
      if (aLanguages[i].internalName == selectedLocale)
        continue;
      var checkbox = document.createElement("checkbox");
      languageList.appendChild(checkbox);
      checkbox.setAttribute("type", "update");
      checkbox.label        = aLanguages[i].name;
      checkbox.URL          = aLanguages[i].URL;
      checkbox.infoURL      = aLanguages[i].infoURL;
      checkbox.internalName = aLanguages[i].internalName;
      uri.spec              = checkbox.URL;
      checkbox.source       = uri.host;
      hasLanguages          = true;
      ++count;
    }
    
    if (hasLanguages) {
      var languagesHeader = document.getElementById("found.languages.header");
      var strings = document.getElementById("updateStrings");
      languagesHeader.label = strings.getFormattedString("updateTypeLangPacks", [count]);
      languagesHeader.collapsed = false;
    }
  },

  _initialized: false,
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, 
                                  "installButtonText", false, 
                                  null, true);
    document.documentElement.getButton("next").focus();
    
    if (this._initialized)
      return;
    this._initialized = true;
    
    var updates = document.getElementById("found.updates");
    updates.computeSizes();

    // Don't show the app update option or critical updates if the user has 
    // already installed an app update but has not yet restarted. 
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    var updatePerformed = pref.getBoolPref(PREF_UPDATE_APP_PERFORMED);

    var updatesvc = Components.classes["@mozilla.org/updates/update-service;1"]
                              .getService(Components.interfaces.nsIUpdateService);
    this._currentInfo = updatesvc.currentVersion;
    if (this._currentInfo) {
      var patches = this._currentInfo.getCollection("patches", { });
      if (patches.length > 0 && !updatePerformed)
        this.buildPatches(patches);
        
      var components = this._currentInfo.getCollection("optional", { });
      if (components.length > 0)
        this.buildOptional(components);

      var languages = this._currentInfo.getCollection("languages", { });
      if (languages.length > 0)
        this.buildLanguages(languages);
    }
    
    this._newestInfo = updatesvc.newestVersion;
    if (this._newestInfo) {
      var languages = this._newestInfo.getCollection("languages", { });
      var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                        .getService(Components.interfaces.nsIXULChromeRegistry);
      var selectedLocale = cr.getSelectedLocale("global");
      var haveLanguage = false;
      for (var i = 0; i < languages.length; ++i) {
        if (languages[i].internalName == selectedLocale)
          haveLanguage = true;
      }

      var files = this._newestInfo.getCollection("files", { });
      if (files.length > 0 && haveLanguage && !updatePerformed) 
        this.buildApp(files);
        
      // When the user upgrades the application, any optional components that
      // they have installed are automatically installed. If there are remaining
      // optional components that are not currently installed, then these
      // are offered as an option.
      var components = this._newestInfo.getCollection("optional", { });
      for (var i = 0; i < components.length; ++i) {
        if (InstallTrigger.getVersion(components[i].internalName))
          gUpdateWizard.appComps.upgraded.optional.push(components[i]);
        else
          gUpdateWizard.appComps.optional.optional.push(components[i]);
      }
      
      var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                         .getService(Components.interfaces.nsIXULChromeRegistry);
      var selectedLocale = cr.getSelectedLocale("global");
      gUpdateWizard.selectedLocaleAvailable = false;
      var languages = this._newestInfo.getCollection("languages", { });
      for (i = 0; i < languages.length; ++i) {
        if (languages[i].internalName == selectedLocale) {
          gUpdateWizard.selectedLocaleAvailable = true;
          gUpdateWizard.appComps.upgraded.languages.push(languages[i]);
        }
      }
      
      if (!gUpdateWizard.selectedLocaleAvailable)
        gUpdateWizard.appComps.optional.languages = gUpdateWizard.appComps.optional.languages.concat(languages);
        
      gUpdateWizard.appComps.upgraded.core = gUpdateWizard.appComps.upgraded.core.concat(files);
    }
    
    this.buildAddons();
    
    var kids = updates._getRadioChildren();
    for (var i = 0; i < kids.length; ++i) {
      if (kids[i].collapsed == false) {
        updates.selectedIndex = i;
        break;
      }
    }
  },
    
  onSelect: function (aEvent)
  {
    var updates = document.getElementById("found.updates");
    var oneChecked = true;
    if (updates.selectedItem.id != "app") {
      oneChecked = false;
      var items = updates.selectedItem.getElementsByTagName("checkbox");
      for (var i = 0; i < items.length; ++i) {  
        if (items[i].checked) {
          oneChecked = true;
          break;
        }
      }
    }

    var strings = document.getElementById("updateStrings");
    var text;
    if (aEvent.target.selectedItem.id == "app") {
      if (gUpdateWizard.appComps.optional.optional.length > 0) {
        gUpdateWizard.setButtonLabels(null, true, 
                                      "nextButtonText", true, 
                                      null, true);
          
        text = strings.getString("foundInstructionsAppComps");
        document.getElementById("found").setAttribute("next", "optional"); 
      }
      gUpdateWizard.updatingApp = true;
    }
    else {
      gUpdateWizard.setButtonLabels(null, true, 
                                    "installButtonText", true, 
                                    null, true);
      text = strings.getString("foundInstructions");
      document.getElementById("found").setAttribute("next", "installing"); 
      
      gUpdateWizard.updatingApp = false;
    }
        
    document.documentElement.getButton("next").disabled = !oneChecked;

    var foundInstructions = document.getElementById("foundInstructions");
    while (foundInstructions.hasChildNodes())
      foundInstructions.removeChild(foundInstructions.firstChild);
    foundInstructions.appendChild(document.createTextNode(text));
  }
};

var gOptionalPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, 
                                  "installButtonText", false, 
                                  null, false);

    var optionalItemsList = document.getElementById("optionalItemsList");
    for (var i = 0; i < gUpdateWizard.appComps.optional.optional.length; ++i) {
      var checkbox = document.createElement("checkbox");
      checkbox.setAttribute("label", gUpdateWizard.appComps.optional.optional[i].name);
      checkbox.setAttribute("index", i);
      optionalItemsList.appendChild(checkbox);
    }
  },
  
  onCommand: function (aEvent)
  {
    if (aEvent.target.localName == "checkbox") {
      gUpdateWizard.appComps.upgraded.optional = [];
      var optionalItemsList = document.getElementById("optionalItemsList");
      var checkboxes = optionalItemsList.getElementsByTagName("checkbox");
      for (var i = 0; i < checkboxes.length; ++i) {
        if (checkboxes[i].checked) {
          var index = parseInt(checkboxes[i].getAttribute("index"));
          var item = gUpdateWizard.appComps.optional.optional[index];
          gUpdateWizard.appComps.upgraded.optional.push(item);
        }
      }
    }
  },
  
  onListMouseOver: function (aEvent)
  {
    if (aEvent.target.localName == "checkbox") {
      var index = parseInt(aEvent.target.getAttribute("index"));
      var desc = gUpdateWizard.appComps.optional.optional[index].description;
      var optionalDescription = document.getElementById("optionalDescription");
      while (optionalDescription.hasChildNodes()) 
        optionalDescription.removeChild(optionalDescription.firstChild);
      optionalDescription.appendChild(document.createTextNode(desc));
    }
  },
  
  onListMouseOut: function (aEvent)
  {
    if (aEvent.target.localName == "vbox") {
      var optionalDescription = document.getElementById("optionalDescription");
      while (optionalDescription.hasChildNodes()) 
        optionalDescription.removeChild(optionalDescription.firstChild);
    }
  }
};

var gInstallingPage = {
  _installing       : false,
  _restartRequired  : false,
  
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, 
                                  "nextButtonText", true, 
                                  null, true);

    // Get XPInstallManager and kick off download/install 
    // process, registering us as an observer. 
    var items = [];
    
    this._restartRequired = false;
    
    gUpdateWizard.remainingExtensionUpdateCount = gUpdateWizard.itemsToUpdate.length;

    var updates = document.getElementById("found.updates");
    if (updates.selectedItem.id != "app") {
      var checkboxes = updates.selectedItem.getElementsByTagName("checkbox");
      for (var i = 0; i < checkboxes.length; ++i) {
        if (checkboxes[i].type == "update" && checkboxes[i].checked) {
          items.push(checkboxes[i].URL);
          this._objs.push({ name: checkboxes[i].label });
        }
      }
    }
    else {
      // To install an app update we need to collect together the following
      // sets of files:
      // - core files
      // - optional components (we need to show another page) 
      // - selected language, if the available language set does not match
      //   the one currently selected for the "global" package
      // Order is *probably* important here.      
      for (var i = 0; i < gUpdateWizard.appComps.upgraded.core.length; ++i) {
        items.push(gUpdateWizard.appComps.upgraded.core[i].URL);
        this._objs.push({ name: gUpdateWizard.appComps.upgraded.core[i].name });
      }
      for (var i = 0; i < gUpdateWizard.appComps.upgraded.languages.length; ++i) {
        items.push(gUpdateWizard.appComps.upgraded.languages[i].URL);
        this._objs.push({ name: gUpdateWizard.appComps.upgraded.languages[i].name });
      }
      for (var i = 0; i < gUpdateWizard.appComps.upgraded.optional.length; ++i) {
        items.push(gUpdateWizard.appComps.upgraded.optional[i].URL);
        this._objs.push({ name: gUpdateWizard.appComps.upgraded.optional[i].name });
      }
    }
    
    var xpimgr = Components.classes["@mozilla.org/xpinstall/install-manager;1"]
                           .createInstance(Components.interfaces.nsIXPInstallManager);
    xpimgr.initManagerFromChrome(items, items.length, this);
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
      default:
        // XXXben ignore chrome registration errors hack!
        if (!(aValue == -239 && gUpdateWizard.updatingApp)) {
          this._objs[aIndex].error = aValue;
          this._errors = true;
        }
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
    
    var iR = document.getElementById("incompatibleRemaining");
    var iR2 = document.getElementById("incompatibleRemaining2");
    var fEC = document.getElementById("finishedEnableChecking");

    if (gUpdateWizard.shouldSuggestAutoChecking) {
      iR.hidden = true;
      iR2.hidden = false;
      fEC.hidden = false;
      fEC.click();
    }
    else {
      iR.hidden = false;
      iR2.hidden = true;
      fEC.hidden = true;
    }
    
    if (gSourceEvent == nsIUpdateService.SOURCE_EVENT_MISMATCH) {
      document.getElementById("finishedMismatch").hidden = false;
      document.getElementById("incompatibleAlert").hidden = false;
    }
  }
};

var gRestartPage = {
  onPageShow: function ()
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    
    // XXXben - we should really have a way to restart the app now from here!
  }
};

var gNoUpdatesPage = {
  onPageShow: function (aEvent)
  {
    gUpdateWizard.setButtonLabels(null, true, null, true, null, true);
    document.documentElement.getButton("finish").focus();
    if (gSourceEvent == nsIUpdateService.SOURCE_EVENT_MISMATCH) {
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

# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
