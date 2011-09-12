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

const kAddonPageSize = 5;

#ifdef ANDROID
const URI_GENERIC_ICON_XPINSTALL = "drawable://alertaddons";
#else
const URI_GENERIC_ICON_XPINSTALL = "chrome://browser/skin/images/alert-addons-30.png";
#endif
const ADDONS_NOTIFICATION_NAME = "addons";

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
      this.hideAlerts();
      let strings = Strings.browser;
      let message = "notificationRestart." + aMode;
      this.showMessage(strings.GetStringFromName(message), "restart-app",
                       strings.GetStringFromName("notificationRestart.button"), false, "addons-restart-app");
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
  },

  delayedInit: function ev__delayedInit() {
    if (this._list)
      return;

    this.init(); // In case the panel is selected before init has been called.

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

    let strings = Strings.browser;
    this._strings["addonType.extension"] = strings.GetStringFromName("addonType.2");
    this._strings["addonType.theme"] = strings.GetStringFromName("addonType.4");
    this._strings["addonType.locale"] = strings.GetStringFromName("addonType.8");
    this._strings["addonType.search"] = strings.GetStringFromName("addonType.1024");

    if (!Services.prefs.getBoolPref("extensions.hideUpdateButton"))
      document.getElementById("addons-update-all").hidden = false;

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

    this.hideAlerts();
  },

  hideOnSelect: function ev_handleEvent(aEvent) {
    // When list selection changes, be sure to close up any open options sections
    if (aEvent.target == this._list)
      this.hideOptions();
  },

  _createLocalAddon: function ev__createLocalAddon(aAddon) {
    let strings = Strings.browser;

    let appManaged = (aAddon.scope == AddonManager.SCOPE_APPLICATION);
    let opType = this._getOpTypeForOperations(aAddon.pendingOperations);
    let updateable = (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE) > 0;
    let uninstallable = (aAddon.permissions & AddonManager.PERM_CAN_UNINSTALL) > 0;

    let blocked = "";
    switch(aAddon.blocklistState) {
      case Ci.nsIBlocklistService.STATE_BLOCKED:
        blocked = strings.getString("addonBlocked.blocked")
        break;
      case Ci.nsIBlocklistService.STATE_SOFTBLOCKED:
        blocked = strings.getString("addonBlocked.softBlocked");
        break;
      case Ci.nsIBlocklistService.STATE_OUTDATED:
        blocked = srings.getString("addonBlocked.outdated");
        break;
    }

    let listitem = this._createItem(aAddon, "local");
    listitem.setAttribute("isDisabled", !aAddon.isActive);
    listitem.setAttribute("appDisabled", aAddon.appDisabled);
    listitem.setAttribute("appManaged", appManaged);
    listitem.setAttribute("description", aAddon.description);
    listitem.setAttribute("optionsURL", aAddon.optionsURL || "");
    listitem.setAttribute("opType", opType);
    listitem.setAttribute("updateable", updateable);
    listitem.setAttribute("isReadonly", !uninstallable);
    if (blocked)
      listitem.setAttribute("blockedStatus", blocked);
    listitem.addon = aAddon;
    return listitem;
  },

  getAddonsFromLocal: function ev_getAddonsFromLocal() {
    this.clearSection("local");

    let self = this;
    AddonManager.getAddonsByTypes(["extension", "theme", "locale"], function(items) {
      let strings = Strings.browser;
      let anyUpdateable = false;
      for (let i = 0; i < items.length; i++) {
        let listitem = self.getElementForAddon(items[i].id)
        if (!listitem)
          listitem = self._createLocalAddon(items[i]);
        if ((items[i].permissions & AddonManager.PERM_CAN_UPGRADE) > 0)
          anyUpdateable = true;

        self.addItem(listitem);
      }

      // Load the search engines
      let defaults = Services.search.getDefaultEngines({ }).map(function (e) e.name);
      function isDefault(aEngine)
        defaults.indexOf(aEngine.name) != -1

      let defaultDescription = strings.GetStringFromName("addonsSearchEngine.description");

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
        self.addItem(listitem);
      }

      if (engines.length + items.length == 0)
        self.displaySectionMessage("local", strings.GetStringFromName("addonsLocalNone.label"), null, true);

      if (!anyUpdateable)
        document.getElementById("addons-update-all").disabled = true;
    });
  },

  addItem : function ev_addItem(aItem, aPosition) {
    if (aPosition == "repo")
      return this._list.appendChild(aItem);
    else if (aPosition == "local")
      return this._list.insertBefore(aItem, this._localItem.nextSibling);
    else
      return this._list.insertBefore(aItem, this._repoItem);
  },

  removeItem : function ev_moveItem(aItem) {
    this._list.removeChild(aItem);
  },

  enable: function ev_enable(aItem) {
    let opType;
    if (aItem.getAttribute("type") == "search") {
      aItem.setAttribute("isDisabled", false);
      aItem._engine.hidden = false;
      opType = "needs-enable";
    } else if (aItem.getAttribute("type") == "theme") {
      // we can have only one theme enabled, so disable the current one if any
      let theme = null;
      let item = this._localItem.nextSibling;
      while (item != this._repoItem) {
        if (item.addon && (item.addon.type == "theme") && (item.addon.isActive)) {
          theme = item;
          break;
        }
        item = item.nextSibling;
      }
      if (theme)
        this.disable(theme);

      aItem.addon.userDisabled = false;
      aItem.setAttribute("isDisabled", false);
    } else {
      aItem.addon.userDisabled = false;
      opType = this._getOpTypeForOperations(aItem.addon.pendingOperations);

      if (aItem.addon.pendingOperations & AddonManager.PENDING_ENABLE) {
        this.showRestart();
      } else {
        aItem.removeAttribute("isDisabled");
        if (aItem.getAttribute("opType") == "needs-disable")
          this.hideRestart();
      };
    }

    aItem.setAttribute("opType", opType);
  },

  disable: function ev_disable(aItem) {
    let opType;
    if (aItem.getAttribute("type") == "search") {
      aItem.setAttribute("isDisabled", true);
      aItem._engine.hidden = true;
      opType = "needs-disable";
    } else if (aItem.getAttribute("type") == "theme") {
      aItem.addon.userDisabled = true;
      aItem.setAttribute("isDisabled", true);
    } else if (aItem.getAttribute("type") == "locale") {
      Services.prefs.clearUserPref("general.useragent.locale");
      aItem.addon.userDisabled = true;
      aItem.setAttribute("isDisabled", true);
    } else {
      aItem.addon.userDisabled = true;
      opType = this._getOpTypeForOperations(aItem.addon.pendingOperations);

      if (aItem.addon.pendingOperations & AddonManager.PENDING_DISABLE) {
        this.showRestart();
      } else {
        aItem.setAttribute("isDisabled", !aItem.addon.isActive);
        if (aItem.getAttribute("opType") == "needs-enable")
          this.hideRestart();
      }
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
      if (!aItem.addon) {
        this._list.removeChild(aItem);
        return;
      }

      aItem.addon.uninstall();
      opType = this._getOpTypeForOperations(aItem.addon.pendingOperations);

      if (aItem.addon.pendingOperations & AddonManager.PENDING_UNINSTALL) {
        this.showRestart();

        // A disabled addon doesn't need a restart so it has no pending ops and
        // can't be cancelled
        if (!aItem.addon.isActive && opType == "")
          opType = "needs-uninstall";

        aItem.setAttribute("opType", opType);
      } else {
        this._list.removeChild(aItem);
      }
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
    if (!aURL)
      return true;

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

    this.addItem(item, aSection);

    return item;
  },

  getAddonsFromRepo: function ev_getAddonsFromRepo(aTerms, aSelectFirstResult) {
    this.clearSection("repo");

    // Make sure we're online before attempting to load
    Util.forceOnline();

    if (AddonRepository.isSearching)
      AddonRepository.cancelSearch();

    let strings = Strings.browser;
    if (aTerms) {
      AddonSearchResults.selectFirstResult = aSelectFirstResult;
      this.displaySectionMessage("repo", strings.GetStringFromName("addonsSearchStart.label"), strings.GetStringFromName("addonsSearchStart.button"), false);
      AddonRepository.searchAddons(aTerms, Services.prefs.getIntPref(PREF_GETADDONS_MAXRESULTS), AddonSearchResults);
    }
    else {
      this.displaySectionMessage("repo", strings.GetStringFromName("addonsSearchStart.label"), strings.GetStringFromName("addonsSearchStart.button"), false);
      AddonRepository.retrieveRecommendedAddons(Services.prefs.getIntPref(PREF_GETADDONS_MAXRESULTS), RecommendedSearchResults);
    }
  },

  appendSearchResults: function(aAddons, aShowRating, aShowCount) {
    let urlproperties = [ "iconURL", "homepageURL" ];
    let foundItem = false;
    let appendedAddons = [];
    for (let i = 0; i < aAddons.length; i++) {
      let addon = aAddons[i];

      // Check for a duplicate add-on, already in the search results
      // (can happen when blending the recommended and browsed lists)
      let element = ExtensionsView.getElementForAddon(addon.install.sourceURI.spec);
      if (element)
        continue;

      // Check for any items with potentially unsafe urls
      if (urlproperties.some(function (p) !this._isSafeURI(addon[p]), this))
        continue;
      if (addon.screenshots && addon.screenshots.length) {
        if (addon.screenshots.some(function (aScreenshot) !this._isSafeURI(aScreenshot), this))
          continue;
      }

      // Convert the numeric type to a string
      let types = {"2":"extension", "4":"theme", "8":"locale"};
      addon.type = types[addon.type];

      let listitem = this._createItem(addon, "search");
      listitem.setAttribute("description", addon.description);
      if (addon.homepageURL)
        listitem.setAttribute("homepageURL", addon.homepageURL);
      listitem.install = addon.install;
      listitem.setAttribute("sourceURL", addon.install.sourceURI.spec);
      if (aShowRating)
        listitem.setAttribute("rating", addon.averageRating);

      let item = this.addItem(listitem, "repo");
      appendedAddons.push(listitem);
      // Hide any overflow add-ons. The user can see them later by pressing the
      // "See More" button
      aShowCount--;
      if (aShowCount < 0)
        item.hidden = true;
    }
    return appendedAddons;
  },

  showMoreSearchResults: function showMoreSearchResults() {
    // Show more add-ons, if we have them
    let showCount = kAddonPageSize;

    // Find the first hidden add-on
    let item = this._repoItem.nextSibling;
    while (item && !item.hidden)
      item = item.nextSibling;

    // Start showing the hidden add-ons
    while (showCount > 0 && item && item.hidden) {
      showCount--;
      item.hidden = false;
      item = item.nextSibling;
    }

    // Hide the "Show More" button if there are no more to show
    if (item == this._list.lastChild)
      item.setAttribute("hidepage", "true");
  },

  displayRecommendedResults: function ev_displaySearchResults(aRecommendedAddons, aBrowseAddons) {
    this.clearSection("repo");

    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
    let browseURL = formatter.formatURLPref("extensions.getAddons.browseAddons");

    let strings = Strings.browser;
    let brandShortName = Strings.brand.GetStringFromName("brandShortName");

    let whatare = document.createElement("richlistitem");
    whatare.setAttribute("typeName", "banner");
    whatare.setAttribute("label", strings.GetStringFromName("addonsWhatAre.label"));

    let desc = strings.GetStringFromName("addonsWhatAre.description");
    desc = desc.replace(/#1/g, brandShortName);
    whatare.setAttribute("description", desc);

    whatare.setAttribute("button", strings.GetStringFromName("addonsWhatAre.button"));
    whatare.setAttribute("onbuttoncommand", "BrowserUI.newTab('" + browseURL + "', Browser.selectedTab);");
    this.addItem(whatare, "repo");

    if (aRecommendedAddons.length == 0 && aBrowseAddons.length == 0) {
      let msg = strings.GetStringFromName("addonsSearchNone.recommended");
      let button = strings.GetStringFromName("addonsSearchNone.button");
      let item = this.displaySectionMessage("repo", msg, button, true);

      this._list.scrollBoxObject.scrollToElement(item);
      return;
    }

    // Locale sensitive sort
    function nameCompare(a, b) {
      return String.localeCompare(a.name, b.name);
    }
    aRecommendedAddons.sort(nameCompare);

    // Rating sort
    function ratingCompare(a, b) {
      return a.averageRating < b.averageRating;
    }
    aBrowseAddons.sort(ratingCompare);

    // We only show extra browse add-ons if the recommended count is small. Otherwise, the user
    // can see more by pressing the "Show More" button
    this.appendSearchResults(aRecommendedAddons, false, aRecommendedAddons.length);
    let minOverflow = (aRecommendedAddons.length >= kAddonPageSize ? 0 : kAddonPageSize);
    let numAdded = this.appendSearchResults(aBrowseAddons, true, minOverflow).length;

    let totalAddons = aRecommendedAddons.length + numAdded;

    let showmore = document.createElement("richlistitem");
    showmore.setAttribute("typeName", "showmore");
    showmore.setAttribute("pagelabel", strings.GetStringFromName("addonsBrowseAll.seeMore"));
    showmore.setAttribute("onpagecommand", "ExtensionsView.showMoreSearchResults();");
    showmore.setAttribute("hidepage", numAdded > minOverflow ? "false" : "true");
    showmore.setAttribute("sitelabel", strings.GetStringFromName("addonsBrowseAll.browseSite"));
    showmore.setAttribute("onsitecommand", "ExtensionsView.showMoreResults('" + browseURL + "');");
    this.addItem(showmore, "repo");

    let evt = document.createEvent("Events");
    evt.initEvent("ViewChanged", true, false);
    this._list.dispatchEvent(evt);
  },

  displaySearchResults: function ev_displaySearchResults(aAddons, aTotalResults, aSelectFirstResult) {
    this.clearSection("repo");

    let strings = Strings.browser;
    if (aAddons.length == 0) {
      let msg = strings.GetStringFromName("addonsSearchNone.search");
      let button = strings.GetStringFromName("addonsSearchSuccess2.button");
      let item = this.displaySectionMessage("repo", msg, button, true);

      if (aSelectFirstResult)
        this._list.scrollBoxObject.scrollToElement(item);
      return;
    }

    let firstAdded = this.appendSearchResults(aAddons, true)[0];
    if (aSelectFirstResult && firstAdded) {
      this._list.selectItem(firstAdded);
      this._list.scrollBoxObject.scrollToElement(firstAdded);
    }

    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);

    if (aTotalResults > aAddons.length) {
      let showmore = document.createElement("richlistitem");
      showmore.setAttribute("typeName", "showmore");
      showmore.setAttribute("hidepage", "true");

      let labelBase = strings.GetStringFromName("addonsSearchMore.label");
      let label = PluralForm.get(aTotalResults, labelBase).replace("#1", aTotalResults);

      showmore.setAttribute("sitelabel", label);

      let url = Services.prefs.getCharPref("extensions.getAddons.search.browseURL");
      url = url.replace(/%TERMS%/g, encodeURIComponent(this.searchBox.value));
      url = formatter.formatURL(url);
      showmore.setAttribute("onsitecommand", "ExtensionsView.showMoreResults('" + url + "');");
      this.addItem(showmore, "repo");
    }

    this.displaySectionMessage("repo", null, strings.GetStringFromName("addonsSearchSuccess2.button"), true);
  },

  showPage: function ev_showPage(aItem) {
    let uri = aItem.getAttribute("homepageURL");
    if (uri)
      BrowserUI.newTab(uri, Browser.selectedTab);
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

  showMoreResults: function ev_showMoreResults(aURL) {
    if (aURL)
      BrowserUI.newTab(aURL, Browser.selectedTab);
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
    let update = JSON.parse(json);

    let strings = Strings.browser;
    let element = this.getElementForAddon(update.id);
    if (!element)
      return;

    let addon = element.addon;

    switch (aTopic) {
      case "addon-update-started":
        element.setAttribute("updateStatus", strings.GetStringFromName("addonUpdate.checking"));
        break;
      case "addon-update-ended":
        let updateable = false;
        let statusMsg = null;
        switch (aData) {
          case "update":
            statusMsg = strings.formatStringFromName("addonUpdate.updating", [update.version], 1);
            updateable = true;
            break;
          case "compatibility":
            if (addon.pendingOperations & AddonManager.PENDING_INSTALL || addon.pendingOperations & AddonManager.PENDING_UPGRADE)
              updateable = true;

            // A compatibility update may require a restart, but will not fire an install
            if (addon.pendingOperations & AddonManager.PENDING_ENABLE &&
                addon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_ENABLE) {
              statusMsg = strings.GetStringFromName("addonUpdate.compatibility");
              this.showRestart();
            }
            break;
          case "error":
            statusMsg = strings.GetStringFromName("addonUpdate.error");
            break;
          case "no-update":
            // Ignore if no updated was found. Just let the message go blank.
            //statusMsg = strings.GetStringFromName("addonUpdate.noupdate");
            break;
          default:
            // Ignore if no updated was found. Just let the message go blank.
            //statusMsg = strings.GetStringFromName("addonUpdate.noupdate");
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
  },

  showAlert: function ev_showAlert(aMessage, aForceDisplay) {
    let strings = Strings.browser;

    let observer = {
      observe: function (aSubject, aTopic, aData) {
        if (aTopic == "alertclickcallback")
          BrowserUI.showPanel("addons-container");
      }
    };

    if (aForceDisplay) {
      // Show a toaster alert for restartless add-ons all the time
      let toaster = Cc["@mozilla.org/toaster-alerts-service;1"].getService(Ci.nsIAlertsService);
      let image = "chrome://browser/skin/images/alert-addons-30.png";
      if (this.visible)
        toaster.showAlertNotification(image, strings.GetStringFromName("alertAddons"), aMessage, false, "", null);
      else
        toaster.showAlertNotification(image, strings.GetStringFromName("alertAddons"), aMessage, true, "", observer);
    } else {
      // Only show an alert for a normal add-on if the manager is not visible
      if (!this.visible) {
        let alerts = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        alerts.showAlertNotification(URI_GENERIC_ICON_XPINSTALL, strings.GetStringFromName("alertAddons"),
                                     aMessage, true, "", observer, ADDONS_NOTIFICATION_NAME);

        // Use a preference to help us cleanup this notification in case we don't shutdown correctly
        Services.prefs.setBoolPref("browser.notifications.pending.addons", true);
        Services.prefs.savePrefFile(null);
      }
    }
  },

  hideAlerts: function ev_hideAlerts() {
#ifdef ANDROID
    let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
    if (progressListener)
      progressListener.onCancel(ADDONS_NOTIFICATION_NAME);
#endif

    // Keep our preference in sync
    Services.prefs.clearUserPref("browser.notifications.pending.addons");
  },
};


function searchFailed() {
  ExtensionsView.clearSection("repo");

  let strings = Strings.browser;
  let brand = Strings.brand;

  let failLabel = strings.formatStringFromName("addonsSearchFail.label",
                                             [brand.GetStringFromName("brandShortName")], 1);
  let failButton = strings.GetStringFromName("addonsSearchFail.retryButton");
  ExtensionsView.displaySectionMessage("repo", failLabel, failButton, true);
}


///////////////////////////////////////////////////////////////////////////////
// callback for the recommended search
var RecommendedSearchResults = {
  cache: null,

  searchSucceeded: function(aAddons, aAddonCount, aTotalResults) {
    this.cache = aAddons;
    AddonRepository.searchAddons(" ", Services.prefs.getIntPref(PREF_GETADDONS_MAXRESULTS), BrowseSearchResults);
  },

  searchFailed: searchFailed
}

///////////////////////////////////////////////////////////////////////////////
// callback for the browse search
var BrowseSearchResults = {
  searchSucceeded: function(aAddons, aAddonCount, aTotalResults) {
    ExtensionsView.displayRecommendedResults(RecommendedSearchResults.cache, aAddons);
  },

  searchFailed: searchFailed
}

///////////////////////////////////////////////////////////////////////////////
// callback for a standard search
var AddonSearchResults = {
  // set by ExtensionsView
  selectFirstResult: false,

  searchSucceeded: function(aAddons, aAddonCount, aTotalResults) {
    ExtensionsView.displaySearchResults(aAddons, aTotalResults, this.selectFirstResult);
  },

  searchFailed: searchFailed
}

///////////////////////////////////////////////////////////////////////////////
// XPInstall download helper
function AddonInstallListener() {
}

AddonInstallListener.prototype = {

  onInstallEnded: function(aInstall, aAddon) {
    let needsRestart = false;
    let mode = "";
    if (aInstall.existingAddon && (aInstall.existingAddon.pendingOperations & AddonManager.PENDING_UPGRADE)) {
      needsRestart = true;
      mode = "update";
    } else if (aAddon.pendingOperations & AddonManager.PENDING_INSTALL) {
      needsRestart = true;
      mode = "normal";
    }

    // if we already have a mode, then we need to show a restart notification
    // otherwise, we are likely a bootstrapped addon
    if (needsRestart)
      ExtensionsView.showRestart(mode);
    this._showInstallCompleteAlert(true, needsRestart);

    // only do this if the view has already been inited
    if (!ExtensionsView._list)
      return;

    let element = ExtensionsView.getElementForAddon(aAddon.id);
    if (!element) {
      element = ExtensionsView._createLocalAddon(aAddon);
      ExtensionsView.addItem(element, "local");
    }

    if (needsRestart) {
      element.setAttribute("opType", "needs-restart");
    } else {
      if (element.getAttribute("typeName") == "search") {
        if (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE)
          document.getElementById("addons-update-all").disabled = false;

        ExtensionsView.removeItem(element);
        element = ExtensionsView._createLocalAddon(aAddon);
        ExtensionsView.addItem(element, "local");
      }
    }

    element.setAttribute("status", "success");

    // If we are updating an add-on, change the status
    if (element.hasAttribute("updating")) {
      let strings = Strings.browser;
      element.setAttribute("updateStatus", strings.formatStringFromName("addonUpdate.updated", [aAddon.version], 1));
      element.removeAttribute("updating");
    }
  },

  onInstallFailed: function(aInstall) {
    this._showInstallCompleteAlert(false);

    if (ExtensionsView.visible) {
      let element = ExtensionsView.getElementForAddon(aInstall.sourceURI.spec);
      if (!element)
        return;

      element.removeAttribute("opType");
      let strings = Services.strings.createBundle("chrome://global/locale/xpinstall/xpinstall.properties");

      let error = null;
      switch (aInstall.error) {
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
        msg = strings.formatStringFromName("unknown.error", [aInstall.error], 1);
      }
      element.setAttribute("error", msg);
    }
  },

  onDownloadProgress: function xpidm_onDownloadProgress(aInstall) {
    let element = ExtensionsView.getElementForAddon(aInstall.sourceURI.spec);

#ifdef ANDROID
    let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
    if (progressListener)
      progressListener.onProgress(ADDONS_NOTIFICATION_NAME, aInstall.progress, aInstall.maxProgress);
#endif

    if (!element)
      return;

    let opType = element.getAttribute("opType");
    if (!opType)
      element.setAttribute("opType", "needs-install");

    let progress = Math.round((aInstall.progress / aInstall.maxProgress) * 100);
    element.setAttribute("progress", progress);
  },

  onDownloadFailed: function(aInstall) {
    this.onInstallFailed(aInstall);
  },

  onDownloadCancelled: function(aInstall) {
    let strings = Strings.browser;
    let brandBundle = Strings.brand;
    let brandShortName = brandBundle.GetStringFromName("brandShortName");
    let host = (aInstall.originatingURI instanceof Ci.nsIStandardURL) && aInstall.originatingURI.host;
    if (!host)
      host = (aInstall.sourceURI instanceof Ci.nsIStandardURL) && aInstall.sourceURI.host;

    let error = (host || aInstall.error == 0) ? "addonError" : "addonLocalError";
    if (aInstall.error != 0)
      error += aInstall.error;
    else if (aInstall.addon && aInstall.addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
      error += "Blocklisted";
    else if (aInstall.addon && (!aInstall.addon.isCompatible || !aInstall.addon.isPlatformCompatible))
      error += "Incompatible";
    else {
      ExtensionsView.hideAlerts();
      return; // no need to show anything in this case
    }

    let messageString = strings.GetStringFromName(error);
    messageString = messageString.replace("#1", aInstall.name);
    if (host)
      messageString = messageString.replace("#2", host);
    messageString = messageString.replace("#3", brandShortName);
    messageString = messageString.replace("#4", Services.appinfo.version);

    ExtensionsView.showAlert(messageString);
  },

  _showInstallCompleteAlert: function xpidm_showAlert(aSucceeded, aNeedsRestart) {
    let strings = Strings.browser;
    let stringName = "alertAddonsFail";
    if (aSucceeded) {
      stringName = "alertAddonsInstalled";
      if (!aNeedsRestart)
        stringName += "NoRestart";
    }

    ExtensionsView.showAlert(strings.GetStringFromName(stringName), !aNeedsRestart);
  },
};
