// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

const PREFIX_ITEM_URI = "urn:mozilla:item:";
const PREFIX_NS_EM = "http://www.mozilla.org/2004/em-rdf#";

const PREF_GETADDONS_MAXRESULTS = "extensions.getAddons.maxResults";

const URI_GENERIC_ICON_XPINSTALL = "chrome://browser/skin/images/alert-addons-30.png";

XPCOMUtils.defineLazyGetter(this, "AddonManager", function() {
  Cu.import("resource://gre/modules/AddonManager.jsm");
  return AddonManager;
});

XPCOMUtils.defineLazyGetter(this, "AddonRepository", function() {
  Cu.import("resource://gre/modules/AddonRepository.jsm");
  return AddonRepository;
});

var ExtensionsView = {
  _strings: {},
  _list: null,
  _localItem: null,
  _repoItem: null,
  _msg: null,
  _dloadmgr: null,
  _restartCount: 0,
  _observerIndex: -1,

  _getOpTypeForOperations: function ev__getOpTypeForOperations(aOperations) {
    if (aOperations & AddonManager.PENDING_UNINSTALL)
      return "needs-uninstall";
    if (aOperations & AddonManager.PENDING_ENABLE)
      return "needs-enable";
    if (aOperations & AddonManager.PENDING_DISABLE)
      return "needs-disable";
    return "";
  },
  
  _createItem: function ev__createItem(aAddon, aTypeName) {
    let item = document.createElement("richlistitem");
    item.setAttribute("id", PREFIX_ITEM_URI + aAddon.id);
    item.setAttribute("addonID", aAddon.id);
    item.setAttribute("typeName", aTypeName);
    item.setAttribute("type", aAddon.type);
    item.setAttribute("typeLabel", this._strings["addonType." + aAddon.type]);
    item.setAttribute("name", aAddon.name);
    item.setAttribute("version", aAddon.version);
    item.setAttribute("iconURL", aAddon.iconURL);
    return item;
  },

  clearSection: function ev_clearSection(aSection) {
    let start = null;
    let end = null;

    if (aSection == "local") {
      start = this._localItem;
      end = this._repoItem;
    }
    else {
      start = this._repoItem;
    }

    while (start.nextSibling != end)
      this._list.removeChild(start.nextSibling);
  },

  _messageActions: function ev__messageActions(aData) {
    if (aData == "addons-restart-app") {
      // Notify all windows that an application quit has been requested
      var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

      // If nothing aborted, quit the app
      if (cancelQuit.data == false) {
        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
      }
    }
  },

  getElementForAddon: function ev_getElementForAddon(aKey) {
    let element = document.getElementById(PREFIX_ITEM_URI + aKey);
    if (!element && this._list)
      element = this._list.getElementsByAttribute("sourceURL", aKey)[0];
    return element;
  },

  showMessage: function ev_showMessage(aMsg, aValue, aButtonLabel, aShowCloseButton, aNotifyData) {
    let notification = this._msg.getNotificationWithValue(aValue);
    if (notification)
      return;

    let self = this;
    let buttons = null;
    if (aButtonLabel) {
      buttons = [ {
        label: aButtonLabel,
        accessKey: "",
        data: aNotifyData,
        callback: function(aNotification, aButton) {
          self._messageActions(aButton.data);
          return true;
        }
      } ];
    }

    this._msg.appendNotification(aMsg, aValue, "", this._msg.PRIORITY_WARNING_LOW, buttons).hideclose = !aShowCloseButton;
  },

  showRestart: function ev_showRestart(aMode) {
    // Increment the count in case the view is not completely initialized
    this._restartCount++;

    // Pick the right message key from the properties file
    aMode = aMode || "normal";

    if (this._msg) {
      let strings = Elements.browserBundle;
      let message = "notificationRestart." + aMode;
      this.showMessage(strings.getString(message), "restart-app",
                       strings.getString("notificationRestart.button"), false, "addons-restart-app");
    }
  },

  hideRestart: function ev_hideRestart() {
    this._restartCount--;
    if (this._restartCount == 0 && this._msg) {
      let notification = this._msg.getNotificationWithValue("restart-app");
      if (notification)
        notification.close();
    }
  },

  showOptions: function ev_showOptions(aID) {
    this.hideOptions();

    let item = this.getElementForAddon(aID);
    if (!item)
      return;

    // if the element is not the selected element, select it
    if (item != this._list.selectedItem)
      this._list.selectedItem = item;

    item.showOptions();
  },

  hideOptions: function ev_hideOptions() {
    if (!this._list)
      return;

    let items = this._list.childNodes;
    for (let i = 0; i < items.length; i++) {
      let item = items[i];
      if (item.hideOptions)
        item.hideOptions();
    }
    this._list.ensureSelectedElementIsVisible();
  },

  get visible() {
    let items = document.getElementById("panel-items");
    if (BrowserUI.isPanelVisible() && items.selectedPanel.id == "addons-container")
      return true;
    return false;
  },

  init: function ev_init() {
    if (this._dloadmgr)
      return;

    this._dloadmgr = new AddonInstallListener();
    AddonManager.addInstallListener(this._dloadmgr);

    // Watch for add-on update notifications
    let os = Services.obs;
    os.addObserver(this, "addon-update-started", false);
    os.addObserver(this, "addon-update-ended", false);

    if (!Services.prefs.getBoolPref("extensions.hideUpdateButton"))
      document.getElementById("addons-update-all").hidden = false;

    let self = this;
    let panels = document.getElementById("panel-items");
    panels.addEventListener("select",
                            function(aEvent) {
                              if (panels.selectedPanel.id == "addons-container")
                                self._delayedInit();
                            },
                            false);
  },

  _delayedInit: function ev__delayedInit() {
    if (this._list)
      return;

    this._list = document.getElementById("addons-list");
    this._localItem = document.getElementById("addons-local");
    this._repoItem = document.getElementById("addons-repo");
    this._msg = document.getElementById("addons-messages");

    // Show the restart notification in case a restart is needed, but the view
    // was not visible at the time
    let notification = this._msg.getNotificationWithValue("restart-app");
    if (this._restartCount > 0 && !notification) {
      this.showRestart();
      this._restartCount--; // showRestart() always increments
    }

    let strings = Elements.browserBundle;
    this._strings["addonType.extension"] = strings.getString("addonType.2");
    this._strings["addonType.theme"] = strings.getString("addonType.4");
    this._strings["addonType.locale"] = strings.getString("addonType.8");
    this._strings["addonType.search"] = strings.getString("addonType.1024");

    let self = this;
    setTimeout(function() {
      self.getAddonsFromLocal();
      self.getAddonsFromRepo("");
    }, 0);
  },

  uninit: function ev_uninit() {
    let os = Services.obs;
    os.removeObserver(this, "addon-update-started");
    os.removeObserver(this, "addon-update-ended");

    AddonManager.removeInstallListener(this._dloadmgr);
  },

  hideOnSelect: function ev_handleEvent(aEvent) {
    // When list selection changes, be sure to close up any open options sections
    if (aEvent.target == this._list)
      this.hideOptions();
  },

  getAddonsFromLocal: function ev_getAddonsFromLocal() {
    this.clearSection("local");

    let self = this;
    AddonManager.getAddonsByTypes(["extension", "theme", "locale"], function(items) {
      for (let i = 0; i < items.length; i++) {
        let addon = items[i];
        let appManaged = (addon.scope == AddonManager.SCOPE_APPLICATION);
        let opType = self._getOpTypeForOperations(addon.pendingOperations);
        let updateable = (addon.permissions & AddonManager.PERM_CAN_UPDATE) > 0;
        let uninstallable = (addon.permissions & AddonManager.PERM_CAN_UNINSTALL) > 0;

        let listitem = self._createItem(addon, "local");
        listitem.setAttribute("isDisabled", !addon.isActive);
        listitem.setAttribute("appDisabled", addon.appDisabled);
        listitem.setAttribute("appManaged", appManaged);
        listitem.setAttribute("description", addon.description);
        listitem.setAttribute("optionsURL", addon.optionsURL);
        listitem.setAttribute("opType", opType);
        listitem.setAttribute("updateable", updateable);
        listitem.setAttribute("isReadonly", !uninstallable);
        listitem.addon = addon;
        self._list.insertBefore(listitem, self._repoItem);
      }

      // Load the search engines
      let defaults = Services.search.getDefaultEngines({ }).map(function (e) e.name);
      function isDefault(aEngine)
        defaults.indexOf(aEngine.name) != -1

      let strings = Elements.browserBundle;
      let defaultDescription = strings.getString("addonsSearchEngine.description");

      let engines = Services.search.getEngines({ });
      for (let e = 0; e < engines.length; e++) {
        let engine = engines[e];
        let addon = {};
        addon.id = engine.name;
        addon.type = "search";
        addon.name = engine.name;
        addon.version = "";
        addon.iconURL = engine.iconURI ? engine.iconURI.spec : "";

        let listitem = self._createItem(addon, "searchplugin");
        listitem._engine = engine;
        listitem.setAttribute("isDisabled", engine.hidden ? "true" : "false");
        listitem.setAttribute("appDisabled", "false");
        listitem.setAttribute("appManaged", isDefault(engine));
        listitem.setAttribute("description", engine.description || defaultDescription);
        listitem.setAttribute("optionsURL", "");
        listitem.setAttribute("opType", engine.hidden ? "needs-disable" : "");
        listitem.setAttribute("updateable", "false");
        self._list.insertBefore(listitem, self._repoItem);
      }

      if (engines.length + items.length == 0) {
        self.displaySectionMessage("local", strings.getString("addonsLocalNone.label"), null, true);
        document.getElementById("addons-update-all").disabled = true;
      }
    });
  },

  enable: function ev_enable(aItem) {
    let opType;
    if (aItem.getAttribute("type") == "search") {
      aItem.setAttribute("isDisabled", false);
      aItem._engine.hidden = false;
      opType = "needs-enable";
    } else {
      aItem.addon.userDisabled = false;
      opType = this._getOpTypeForOperations(aItem.addon.pendingOperations);

      if (opType == "needs-enable")
        this.showRestart();
      else
        this.hideRestart();
    }

    aItem.setAttribute("opType", opType);
  },

  disable: function ev_disable(aItem) {
    let opType;
    if (aItem.getAttribute("type") == "search") {
      aItem.setAttribute("isDisabled", true);
      aItem._engine.hidden = true;
      opType = "needs-disable";
    } else {
      aItem.addon.userDisabled = true;
      opType = this._getOpTypeForOperations(aItem.addon.pendingOperations);

      if (opType == "needs-disable")
        this.showRestart();
      else
        this.hideRestart();
    }

    aItem.setAttribute("opType", opType);
  },

  uninstall: function ev_uninstall(aItem) {
    let opType;
    if (aItem.getAttribute("type") == "search") {
      // Make sure the engine isn't hidden before removing it, to make sure it's
      // visible if the user later re-adds it (works around bug 341833)
      aItem._engine.hidden = false;
      Services.search.removeEngine(aItem._engine);
      // the search-engine-modified observer in browser.js will take care of
      // updating the list
    } else {
      aItem.addon.uninstall();
      opType = this._getOpTypeForOperations(aItem.addon.pendingOperations);

      if (opType == "needs-uninstall")
        this.showRestart();
      aItem.setAttribute("opType", opType);
    }
  },

  cancelUninstall: function ev_cancelUninstall(aItem) {
    aItem.addon.cancelUninstall();

    this.hideRestart();

    let opType = this._getOpTypeForOperations(aItem.addon.pendingOperations);
    aItem.setAttribute("opType", opType);
  },

  installFromRepo: function ev_installFromRepo(aItem) {
    aItem.install.install();

    // display the progress bar early
    let opType = aItem.getAttribute("opType");
    if (!opType)
      aItem.setAttribute("opType", "needs-install");
  },

  _isSafeURI: function ev_isSafeURI(aURL) {
    try {
      var uri = Services.io.newURI(aURL, null, null);
      var scheme = uri.scheme;
    } catch (ex) {}
    return (uri && (scheme == "http" || scheme == "https" || scheme == "ftp"));
  },

  displaySectionMessage: function ev_displaySectionMessage(aSection, aMessage, aButtonLabel, aHideThrobber) {
    let item = document.createElement("richlistitem");
    item.setAttribute("typeName", "message");
    item.setAttribute("message", aMessage);
    if (aButtonLabel)
      item.setAttribute("button", aButtonLabel);
    else
      item.setAttribute("hidebutton", "true");
    item.setAttribute("hidethrobber", aHideThrobber);

    if (aSection == "repo")
      this._list.appendChild(item);
    else
      this._list.insertBefore(item, this._repoItem);

    return item;
  },

  getAddonsFromRepo: function ev_getAddonsFromRepo(aTerms, aSelectFirstResult) {
    this.clearSection("repo");

    // Make sure we're online before attempting to load
    Util.forceOnline();

    if (AddonRepository.isSearching)
      AddonRepository.cancelSearch();

    let strings = Elements.browserBundle;
    if (aTerms) {
      AddonSearchResults.selectFirstResult = aSelectFirstResult;
      this.displaySectionMessage("repo", strings.getString("addonsSearchStart.label"),
                                strings.getString("addonsSearchStart.button"), false);
      AddonRepository.searchAddons(aTerms, Services.prefs.getIntPref(PREF_GETADDONS_MAXRESULTS), AddonSearchResults);
    }
    else {
      if (RecommendedSearchResults.cache) {
        this.displaySearchResults(RecommendedSearchResults.cache, -1, true);
      }
      else {
        this.displaySectionMessage("repo", strings.getString("addonsSearchStart.label"),
                                  strings.getString("addonsSearchStart.button"), false);
        AddonRepository.retrieveRecommendedAddons(Services.prefs.getIntPref(PREF_GETADDONS_MAXRESULTS), RecommendedSearchResults);
      }
    }
  },

  displaySearchResults: function ev_displaySearchResults(aAddons, aTotalResults, aIsRecommended, aSelectFirstResult) {
    this.clearSection("repo");

    let strings = Elements.browserBundle;
    if (aAddons.length == 0) {
      let msg = aIsRecommended ? strings.getString("addonsSearchNone.recommended") :
                                 strings.getString("addonsSearchNone.search");
      let button = aIsRecommended ? strings.getString("addonsSearchNone.button") :
                                    strings.getString("addonsSearchSuccess2.button");
      let item = this.displaySectionMessage("repo", msg, button, true);

      if (aSelectFirstResult)
        this._list.scrollBoxObject.scrollToElement(item);
      return;
    }

    if (aIsRecommended) {
      // Locale sensitive sort
      function compare(a, b) {
        return String.localeCompare(a.name, b.name);
      }
      aAddons.sort(compare);
    }

    var urlproperties = [ "iconURL", "homepageURL" ];
    var properties = [ "name", "iconURL", "homepageURL", "screenshots" ];
    var foundItem = false;
    for (let i = 0; i < aAddons.length; i++) {
      let addon = aAddons[i];

      // Check for any items with potentially unsafe urls
      if (urlproperties.some(function (p) !this._isSafeURI(addon[p]), this))
        continue;
      if (addon.screenshots &&
          addon.screenshots.some(function (aScreenshot) !this._isSafeURI(aScreenshot), this))
        continue;

      // Convert the numeric type to a string
      let types = {"2":"extension", "4":"theme", "8":"locale"};
      addon.type = types[addon.type];

      let listitem = this._createItem(addon, "search");
      listitem.setAttribute("description", addon.description);
      listitem.setAttribute("homepageURL", addon.homepageURL);
      listitem.install = addon.install;
      listitem.setAttribute("sourceURL", addon.install.sourceURL);
      if (!aIsRecommended)
        listitem.setAttribute("rating", addon.rating);
      let item = this._list.appendChild(listitem);

      if (aSelectFirstResult && !foundItem) {
        foundItem = true;
        this._list.selectItem(item);
        this._list.scrollBoxObject.scrollToElement(item);
      }
    }

    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);

    if (!aIsRecommended) {
      if (aTotalResults > aAddons.length) {
        let showmore = document.createElement("richlistitem");
        showmore.setAttribute("typeName", "showmore");

        let labelBase = strings.getString("addonsSearchMore.label");
        let label = PluralForm.get(aTotalResults, labelBase).replace("#1", aTotalResults);
        showmore.setAttribute("label", label);

        let url = Services.prefs.getCharPref("extensions.getAddons.search.browseURL");
        url = url.replace(/%TERMS%/g, encodeURIComponent(this.searchBox.value));
        url = formatter.formatURL(url);
        showmore.setAttribute("url", url);
        this._list.appendChild(showmore);
      }

      this.displaySectionMessage("repo", null, strings.getString("addonsSearchSuccess2.button"), true);
    } else {
      let showmore = document.createElement("richlistitem");
      showmore.setAttribute("typeName", "showmore");
      showmore.setAttribute("label", strings.getString("addonsBrowseAll.label"));

      let url = formatter.formatURLPref("extensions.getAddons.browseAddons");
      showmore.setAttribute("url", url);
      this._list.appendChild(showmore);
    }
  },

  showPage: function ev_showPage(aItem) {
    let uri = aItem.getAttribute("homepageURL");
    if (uri)
      BrowserUI.newTab(uri);
  },

  get searchBox() {
    delete this.searchBox;
    return this.searchBox = document.getElementById("addons-search-text");
  },

  doSearch: function ev_doSearch(aTerms) {
    this.searchBox.value = aTerms;
    this.getAddonsFromRepo(aTerms, true);
  },

  resetSearch: function ev_resetSearch() {
    this.searchBox.value = "";
    this.getAddonsFromRepo("");
  },

  showMoreResults: function ev_showMoreResults(aItem) {
    let uri = aItem.getAttribute("url");
    if (uri)
      BrowserUI.newTab(uri);
  },

  updateAll: function ev_updateAll() {
    let aus = Cc["@mozilla.org/browser/addon-update-service;1"].getService(Ci.nsITimerCallback);
    aus.notify(null);
 
    if (this._list.selectedItem)
      this._list.selectedItem.focus();
  },

  observe: function ev_observe(aSubject, aTopic, aData) {
    if (!document)
      return;

    let json = aSubject.QueryInterface(Ci.nsISupportsString).data;
    let addon = JSON.parse(json);

    let strings = Elements.browserBundle;
    let element = this.getElementForAddon(addon.id);
    if (!element)
      return;

    switch (aTopic) {
      case "addon-update-started":
        element.setAttribute("updateStatus", strings.getString("addonUpdate.checking"));
        break;
      case "addon-update-ended":
        let updateable = false;
        let statusMsg = null;
        switch (aData) {
          case "update":
            statusMsg = strings.getFormattedString("addonUpdate.updating", [addon.version]);
            updateable = true;
            break;
          case "compatibility":
            statusMsg = strings.getString("addonUpdate.compatibility");
            break;
          case "error":
            statusMsg = strings.getString("addonUpdate.error");
            break;
          case "no-update":
            // Ignore if no updated was found. Just let the message go blank.
            //statusMsg = strings.getString("addonUpdate.noupdate");
            break;
          default:
            // Ignore if no updated was found. Just let the message go blank.
            //statusMsg = strings.getString("addonUpdate.noupdate");
        }

        if (statusMsg)
          element.setAttribute("updateStatus", statusMsg);
        else
          element.removeAttribute("updateStatus");

        // Tag the add-on so the AddonInstallListener knows it's an update
        if (updateable)
          element.setAttribute("updating", "true");
        break;
    }
  }
};


function searchFailed() {
  ExtensionsView.clearSection("repo");

  let strings = Elements.browserBundle;
  let brand = document.getElementById("bundle_brand");

  let failLabel = strings.getFormattedString("addonsSearchFail.label",
                                             [brand.getString("brandShortName")]);
  let failButton = strings.getString("addonsSearchFail.button");
  ExtensionsView.displaySectionMessage("repo", failLabel, failButton, true);
}


///////////////////////////////////////////////////////////////////////////////
// callback for the recommended search
var RecommendedSearchResults = {
  cache: null,

  searchSucceeded: function(aAddons, aAddonCount, aTotalResults) {
    this.cache = aAddons;
    ExtensionsView.displaySearchResults(aAddons, aTotalResults, true);
  },

  searchFailed: searchFailed
}

///////////////////////////////////////////////////////////////////////////////
// callback for a standard search
var AddonSearchResults = {
  // set by ExtensionsView
  selectFirstResult: false,

  searchSucceeded: function(aAddons, aAddonCount, aTotalResults) {
    ExtensionsView.displaySearchResults(aAddons, aTotalResults, false, this.selectFirstResult);
  },

  searchFailed: searchFailed
}

///////////////////////////////////////////////////////////////////////////////
// XPInstall download helper
function AddonInstallListener() {
}

AddonInstallListener.prototype = {
  _updating: false,
  onInstallEnded: function(aInstall, aAddon) {
    // XXX fix updating stuff
    if (aAddon.pendingOperations & AddonManager.PENDING_INSTALL)
      ExtensionsView.showRestart(this._updating ? "update" : "normal");

    this._showAlert(true);

    if (!ExtensionsView.visible)
      return;

    let element = ExtensionsView.getElementForAddon(aInstall.sourceURL);
    if (!element)
      return;

    element.setAttribute("opType", "needs-restart");
    element.setAttribute("status", "success");

    // If we are updating an add-on, change the status
    if (element.hasAttribute("updating")) {
      let strings = Elements.browserBundle;
      element.setAttribute("updateStatus", strings.getFormattedString("addonUpdate.updated", [aAddon.version]));
      element.removeAttribute("updating");

      // Remember that we are updating so we can customize the restart message
      this._updating = true;
    }
  },

  onInstallFailed: function(aInstall, aError) {
    this._showAlert(false);

    if (ExtensionsView.visible) {
      let element = ExtensionsView.getElementForAddon(aInstall.sourceURL);
      if (!element)
        return;
  
      element.removeAttribute("opType");
      let strings = Services.strings.createBundle("chrome://global/locale/xpinstall/xpinstall.properties");

      let error = null;
      switch (aError) {
      case AddonManager.ERROR_NETWORK_FAILURE:
        error = "error-228";
        break;
      case AddonManager.ERROR_INCORRECT_HASH:
        error = "error-261";
        break;
      case AddonManager.ERROR_CORRUPT_FILE:
        error = "error-207";
        break;
      }

      try {
        var msg = strings.GetStringFromName(error);
      } catch (ex) {
        msg = strings.formatStringFromName("unknown.error", [aError]);
      }
      element.setAttribute("error", msg);
    }
  },

  onDownloadProgress: function xpidm_onDownloadProgress(aInstall) {
    var element = ExtensionsView.getElementForAddon(aInstall.sourceURL);
    if (!element)
      return;

    let opType = element.getAttribute("opType");
    if (!opType)
      element.setAttribute("opType", "needs-install");

    let progress = Math.round((aInstall.progress / aInstall.maxProgress) * 100);
    element.setAttribute("progress", progress);
  },

  onDownloadFailed: function(aInstall, aError) {
    this.onInstallFailed(aInstall, aError);
  },

  _showAlert: function xpidm_showAlert(aSucceeded) {
    if (ExtensionsView.visible)
      return;

    let strings = Elements.browserBundle;
    let message = aSucceeded ? strings.getString("alertAddonsInstalled") :
                               strings.getString("alertAddonsFail");

    let observer = {
      observe: function (aSubject, aTopic, aData) {
        if (aTopic == "alertclickcallback")
          BrowserUI.showPanel("addons-container");
      }
    };

    let alerts = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    alerts.showAlertNotification(URI_GENERIC_ICON_XPINSTALL, strings.getString("alertAddons"),
                                 message, true, "", observer);
  }
};
