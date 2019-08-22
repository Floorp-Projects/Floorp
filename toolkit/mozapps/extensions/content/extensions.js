/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../content/contentAreaUtils.js */
/* import-globals-from aboutaddonsCommon.js */
/* globals ProcessingInstruction */
/* exported gBrowser, loadView */

const { DeferredTask } = ChromeUtils.import(
  "resource://gre/modules/DeferredTask.jsm"
);
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { AddonRepository } = ChromeUtils.import(
  "resource://gre/modules/addons/AddonRepository.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AMTelemetry",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionPermissions",
  "resource://gre/modules/ExtensionPermissions.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PluralForm",
  "resource://gre/modules/PluralForm.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ClientID",
  "resource://gre/modules/ClientID.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "XPINSTALL_ENABLED",
  "xpinstall.enabled",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "allowPrivateBrowsingByDefault",
  "extensions.allowPrivateBrowsingByDefault",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SUPPORT_URL",
  "app.support.baseURL",
  "",
  null,
  val => Services.urlFormatter.formatURL(val)
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useHtmlViews",
  "extensions.htmlaboutaddons.enabled"
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useHtmlDiscover",
  "extensions.htmlaboutaddons.discover.enabled"
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useNewAboutDebugging",
  "devtools.aboutdebugging.new-enabled"
);

const PREF_DISCOVERURL = "extensions.webservice.discoverURL";
const PREF_DISCOVER_ENABLED = "extensions.getAddons.showPane";
const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_GETADDONS_CACHE_ID_ENABLED =
  "extensions.%ID%.getAddons.cache.enabled";
const PREF_UI_TYPE_HIDDEN = "extensions.ui.%TYPE%.hidden";
const PREF_UI_LASTCATEGORY = "extensions.ui.lastCategory";
const PREF_LEGACY_EXCEPTIONS = "extensions.legacy.exceptions";
const PREF_LEGACY_ENABLED = "extensions.legacy.enabled";

const LOADING_MSG_DELAY = 100;

const UPDATES_RECENT_TIMESPAN = 2 * 24 * 3600000; // 2 days (in milliseconds)

var gViewDefault = "addons://discover/";

XPCOMUtils.defineLazyGetter(this, "extensionStylesheets", () => {
  const { ExtensionParent } = ChromeUtils.import(
    "resource://gre/modules/ExtensionParent.jsm"
  );
  return ExtensionParent.extensionStylesheets;
});

var gStrings = {};
XPCOMUtils.defineLazyServiceGetter(
  gStrings,
  "bundleSvc",
  "@mozilla.org/intl/stringbundle;1",
  "nsIStringBundleService"
);

XPCOMUtils.defineLazyGetter(gStrings, "brand", function() {
  return this.bundleSvc.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});
XPCOMUtils.defineLazyGetter(gStrings, "ext", function() {
  return this.bundleSvc.createBundle(
    "chrome://mozapps/locale/extensions/extensions.properties"
  );
});
XPCOMUtils.defineLazyGetter(gStrings, "dl", function() {
  return this.bundleSvc.createBundle(
    "chrome://mozapps/locale/downloads/downloads.properties"
  );
});

XPCOMUtils.defineLazyGetter(gStrings, "brandShortName", function() {
  return this.brand.GetStringFromName("brandShortName");
});
XPCOMUtils.defineLazyGetter(gStrings, "appVersion", function() {
  return Services.appinfo.version;
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "legacyWarningExceptions",
  PREF_LEGACY_EXCEPTIONS,
  "",
  raw => raw.split(",")
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "legacyExtensionsEnabled",
  PREF_LEGACY_ENABLED,
  true,
  () => gLegacyView.refreshVisibility()
);

document.addEventListener("load", initialize, true);
window.addEventListener("unload", shutdown);

var gPendingInitializations = 1;
Object.defineProperty(this, "gIsInitializing", {
  get: () => gPendingInitializations > 0,
});

function initialize(event) {
  // XXXbz this listener gets _all_ load events for all nodes in the
  // document... but relies on not being called "too early".
  if (event.target instanceof ProcessingInstruction) {
    return;
  }
  document.removeEventListener("load", initialize, true);

  let globalCommandSet = document.getElementById("globalCommandSet");
  globalCommandSet.addEventListener("command", function(event) {
    gViewController.doCommand(event.target.id);
  });

  let viewCommandSet = document.getElementById("viewCommandSet");
  viewCommandSet.addEventListener("commandupdate", function(event) {
    gViewController.updateCommands();
  });
  viewCommandSet.addEventListener("command", function(event) {
    gViewController.doCommand(event.target.id);
  });

  let addonPage = document.getElementById("addons-page");
  addonPage.addEventListener("dragenter", function(event) {
    gDragDrop.onDragOver(event);
  });
  addonPage.addEventListener("dragover", function(event) {
    gDragDrop.onDragOver(event);
  });
  addonPage.addEventListener("drop", function(event) {
    gDragDrop.onDrop(event);
  });
  addonPage.addEventListener("keypress", function(event) {
    gHeader.onKeyPress(event);
  });
  if (!isDiscoverEnabled()) {
    gViewDefault = "addons://list/extension";
  }

  let helpButton = document.getElementById("helpButton");
  let helpUrl =
    Services.urlFormatter.formatURLPref("app.support.baseURL") + "addons-help";
  helpButton.setAttribute("href", helpUrl);
  helpButton.addEventListener("click", () => recordLinkTelemetry("support"));

  document.getElementById("preferencesButton").addEventListener("click", () => {
    let mainWindow = window.windowRoot.ownerGlobal;
    recordLinkTelemetry("about:preferences");
    if ("switchToTabHavingURI" in mainWindow) {
      mainWindow.switchToTabHavingURI("about:preferences", true, {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }
  });

  let categories = document.getElementById("categories");
  document.addEventListener("keydown", () => {
    categories.setAttribute("keyboard-navigation", "true");
  });
  categories.addEventListener("mousedown", () => {
    categories.removeAttribute("keyboard-navigation");
  });

  gViewController.initialize();
  gCategories.initialize();
  gHeader.initialize();
  gEventManager.initialize();
  Services.obs.addObserver(sendEMPong, "EM-ping");
  Services.obs.notifyObservers(window, "EM-loaded");

  if (!XPINSTALL_ENABLED) {
    document.getElementById("cmd_installFromFile").hidden = true;
  }

  // If the initial view has already been selected (by a call to loadView from
  // the above notifications) then bail out now
  if (gViewController.initialViewSelected) {
    return;
  }

  // If there is a history state to restore then use that
  if (window.history.state) {
    gViewController.updateState(window.history.state);
    return;
  }

  // Default to the last selected category
  var view = gCategories.node.value;

  // Allow passing in a view through the window arguments
  if (
    "arguments" in window &&
    window.arguments.length > 0 &&
    window.arguments[0] !== null &&
    "view" in window.arguments[0]
  ) {
    view = window.arguments[0].view;
  }

  gViewController.loadInitialView(view);
}

function notifyInitialized() {
  if (!gIsInitializing) {
    return;
  }

  gPendingInitializations--;
  if (!gIsInitializing) {
    var event = document.createEvent("Events");
    event.initEvent("Initialized", true, true);
    document.dispatchEvent(event);
  }
}

function shutdown() {
  gCategories.shutdown();
  gEventManager.shutdown();
  gViewController.shutdown();
  Services.obs.removeObserver(sendEMPong, "EM-ping");
}

function sendEMPong(aSubject, aTopic, aData) {
  Services.obs.notifyObservers(window, "EM-pong");
}

function recordLinkTelemetry(target) {
  let extra = { view: getCurrentViewName() };
  if (target == "search") {
    let searchBar = document.getElementById("header-search");
    extra.type = searchBar.getAttribute("data-addon-type");
  }
  AMTelemetry.recordLinkEvent({ object: "aboutAddons", value: target, extra });
}

function recordActionTelemetry({ action, value, view, addon }) {
  view = view || getCurrentViewName();
  AMTelemetry.recordActionEvent({
    // The max-length for an object is 20, which it enough to be unique.
    object: "aboutAddons",
    value,
    view,
    action,
    addon,
  });
}

async function recordViewTelemetry(param) {
  let type;
  let addon;

  if (
    param in AddonManager.addonTypes ||
    ["recent", "available"].includes(param)
  ) {
    type = param;
  } else if (param) {
    let id = param.replace("/preferences", "");
    addon = await AddonManager.getAddonByID(id);
  }

  AMTelemetry.recordViewEvent({ view: getCurrentViewName(), addon, type });
}

function recordSetUpdatePolicyTelemetry() {
  // Record telemetry for changing the update policy.
  let updatePolicy = [];
  if (AddonManager.autoUpdateDefault) {
    updatePolicy.push("default");
  }
  if (AddonManager.updateEnabled) {
    updatePolicy.push("enabled");
  }
  recordActionTelemetry({
    action: "setUpdatePolicy",
    value: updatePolicy.join(","),
  });
}

function recordSetAddonUpdateTelemetry(addon) {
  let updates = addon.applyBackgroundUpdates;
  let updatePolicy = "";
  if (updates == "1") {
    updatePolicy = "default";
  } else if (updates == "2") {
    updatePolicy = "enabled";
  }
  recordActionTelemetry({
    action: "setAddonUpdate",
    value: updatePolicy,
    addon,
  });
}

function getCurrentViewName() {
  let view = gViewController.currentViewObj;
  let entries = Object.entries(gViewController.viewObjects);
  let viewIndex = entries.findIndex(([name, viewObj]) => {
    return viewObj == view;
  });
  if (viewIndex != -1) {
    return entries[viewIndex][0];
  }
  return "other";
}

// Used by external callers to load a specific view into the manager
function loadView(aViewId) {
  if (!gViewController.initialViewSelected) {
    // The caller opened the window and immediately loaded the view so it
    // should be the initial history entry

    gViewController.loadInitialView(aViewId);
  } else {
    gViewController.loadView(aViewId);
  }
}

function isLegacyExtension(addon) {
  let legacy = false;
  if (addon.type == "extension" && !addon.isWebExtension) {
    legacy = true;
  }
  if (addon.type == "theme") {
    legacy = false;
  }

  if (
    legacy &&
    (addon.hidden || addon.signedState == AddonManager.SIGNEDSTATE_PRIVILEGED)
  ) {
    legacy = false;
  }
  // Exceptions that can slip through above: the default theme plus
  // test pilot addons until we get SIGNEDSTATE_PRIVILEGED deployed.
  if (legacy && legacyWarningExceptions.includes(addon.id)) {
    legacy = false;
  }
  return legacy;
}

function isDisabledLegacy(addon) {
  return !legacyExtensionsEnabled && isLegacyExtension(addon);
}

function isDiscoverEnabled() {
  if (
    Services.prefs.getPrefType(PREF_DISCOVERURL) == Services.prefs.PREF_INVALID
  ) {
    return false;
  }

  try {
    if (!Services.prefs.getBoolPref(PREF_DISCOVER_ENABLED)) {
      return false;
    }
  } catch (e) {}

  if (!XPINSTALL_ENABLED) {
    return false;
  }

  return true;
}

function setSearchLabel(type) {
  let searchLabel = document.getElementById("search-label");
  document
    .getElementById("header-search")
    .setAttribute("data-addon-type", type);
  let keyMap = {
    extension: "extension",
    shortcuts: "extension",
    theme: "theme",
  };
  if (type in keyMap) {
    searchLabel.textContent = gStrings.ext.GetStringFromName(
      `searchLabel.${keyMap[type]}`
    );
    searchLabel.hidden = false;
  } else {
    searchLabel.textContent = "";
    searchLabel.hidden = true;
  }
}

/**
 * A wrapper around the HTML5 session history service that allows the browser
 * back/forward controls to work within the manager
 */
var HTML5History = {
  get index() {
    return window.docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory
      .index;
  },

  get canGoBack() {
    return window.docShell.QueryInterface(Ci.nsIWebNavigation).canGoBack;
  },

  get canGoForward() {
    return window.docShell.QueryInterface(Ci.nsIWebNavigation).canGoForward;
  },

  back() {
    window.history.back();
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  forward() {
    window.history.forward();
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  pushState(aState) {
    window.history.pushState(aState, document.title);
  },

  replaceState(aState) {
    window.history.replaceState(aState, document.title);
  },

  popState() {
    function onStatePopped(aEvent) {
      window.removeEventListener("popstate", onStatePopped, true);
      // TODO To ensure we can't go forward again we put an additional entry
      // for the current state into the history. Ideally we would just strip
      // the history but there doesn't seem to be a way to do that. Bug 590661
      window.history.pushState(aEvent.state, document.title);
    }
    window.addEventListener("popstate", onStatePopped, true);
    window.history.back();
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },
};

/**
 * A wrapper around a fake history service
 */
var FakeHistory = {
  pos: 0,
  states: [null],

  get index() {
    return this.pos;
  },

  get canGoBack() {
    return this.pos > 0;
  },

  get canGoForward() {
    return this.pos + 1 < this.states.length;
  },

  back() {
    if (this.pos == 0) {
      throw Components.Exception("Cannot go back from this point");
    }

    this.pos--;
    gViewController.updateState(this.states[this.pos]);
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  forward() {
    if (this.pos + 1 >= this.states.length) {
      throw Components.Exception("Cannot go forward from this point");
    }

    this.pos++;
    gViewController.updateState(this.states[this.pos]);
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  pushState(aState) {
    this.pos++;
    this.states.splice(this.pos, this.states.length);
    this.states.push(aState);
  },

  replaceState(aState) {
    this.states[this.pos] = aState;
  },

  popState() {
    if (this.pos == 0) {
      throw Components.Exception("Cannot popState from this view");
    }

    this.states.splice(this.pos, this.states.length);
    this.pos--;

    gViewController.updateState(this.states[this.pos]);
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },
};

// If the window has a session history then use the HTML5 History wrapper
// otherwise use our fake history implementation
if (window.docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory) {
  var gHistory = HTML5History;
} else {
  gHistory = FakeHistory;
}

var gEventManager = {
  _listeners: {},
  _installListeners: new Set(),

  initialize() {
    const ADDON_EVENTS = [
      "onEnabling",
      "onEnabled",
      "onDisabling",
      "onDisabled",
      "onUninstalling",
      "onUninstalled",
      "onInstalled",
      "onOperationCancelled",
      "onUpdateAvailable",
      "onUpdateFinished",
      "onCompatibilityUpdateAvailable",
      "onPropertyChanged",
    ];
    for (let evt of ADDON_EVENTS) {
      let event = evt;
      this[event] = (...aArgs) => this.delegateAddonEvent(event, aArgs);
    }

    const INSTALL_EVENTS = [
      "onNewInstall",
      "onDownloadStarted",
      "onDownloadEnded",
      "onDownloadFailed",
      "onDownloadProgress",
      "onDownloadCancelled",
      "onInstallStarted",
      "onInstallEnded",
      "onInstallFailed",
      "onInstallCancelled",
      "onExternalInstall",
    ];
    for (let evt of INSTALL_EVENTS) {
      let event = evt;
      this[event] = (...aArgs) => this.delegateInstallEvent(event, aArgs);
    }

    AddonManager.addManagerListener(this);
    AddonManager.addInstallListener(this);
    AddonManager.addAddonListener(this);

    this.refreshGlobalWarning();
    this.refreshAutoUpdateDefault();

    var contextMenu = document.getElementById("addonitem-popup");
    contextMenu.addEventListener("popupshowing", function() {
      var addon = gViewController.currentViewObj.getSelectedAddon();
      contextMenu.setAttribute("addontype", addon.type);

      var menuSep = document.getElementById("addonitem-menuseparator");
      var countMenuItemsBeforeSep = 0;
      for (let child of contextMenu.children) {
        if (child == menuSep) {
          break;
        }
        if (
          child.nodeName == "menuitem" &&
          gViewController.isCommandEnabled(child.command)
        ) {
          countMenuItemsBeforeSep++;
        }
      }

      // Hide the separator if there are no visible menu items before it
      menuSep.hidden = countMenuItemsBeforeSep == 0;
    });

    let addonTooltip = document.getElementById("addonitem-tooltip");
    addonTooltip.addEventListener("popupshowing", function() {
      let addonItem = addonTooltip.triggerNode;
      // The way the test triggers the tooltip the richlistitem is the
      // tooltipNode but in normal use it is the anonymous node. This allows
      // any case
      if (addonItem.localName != "richlistitem") {
        addonItem = document.getBindingParent(addonItem);
      }

      let tiptext = addonItem.getAttribute("name");

      if (addonItem.mAddon) {
        if (shouldShowVersionNumber(addonItem.mAddon)) {
          tiptext +=
            " " +
            (addonItem.hasAttribute("upgrade")
              ? addonItem.mManualUpdate.version
              : addonItem.mAddon.version);
        }
      } else if (shouldShowVersionNumber(addonItem.mInstall)) {
        tiptext += " " + addonItem.mInstall.version;
      }

      addonTooltip.label = tiptext;
    });
  },

  shutdown() {
    AddonManager.removeManagerListener(this);
    AddonManager.removeInstallListener(this);
    AddonManager.removeAddonListener(this);
  },

  registerAddonListener(aListener, aAddonId) {
    if (!(aAddonId in this._listeners)) {
      this._listeners[aAddonId] = new Set();
    }
    this._listeners[aAddonId].add(aListener);
  },

  unregisterAddonListener(aListener, aAddonId) {
    if (!(aAddonId in this._listeners)) {
      return;
    }
    this._listeners[aAddonId].delete(aListener);
  },

  registerInstallListener(aListener) {
    this._installListeners.add(aListener);
  },

  unregisterInstallListener(aListener) {
    this._installListeners.delete(aListener);
  },

  delegateAddonEvent(aEvent, aParams) {
    var addon = aParams.shift();
    if (!(addon.id in this._listeners)) {
      return;
    }

    function tryListener(listener) {
      if (!(aEvent in listener)) {
        return;
      }
      try {
        listener[aEvent].apply(listener, aParams);
      } catch (e) {
        // this shouldn't be fatal
        Cu.reportError(e);
      }
    }

    for (let listener of this._listeners[addon.id]) {
      tryListener(listener);
    }
    // eslint-disable-next-line dot-notation
    for (let listener of this._listeners["ANY"]) {
      tryListener(listener);
    }
  },

  delegateInstallEvent(aEvent, aParams) {
    var existingAddon =
      aEvent == "onExternalInstall" ? aParams[1] : aParams[0].existingAddon;
    // If the install is an update then send the event to all listeners
    // registered for the existing add-on
    if (existingAddon) {
      this.delegateAddonEvent(aEvent, [existingAddon].concat(aParams));
    }

    for (let listener of this._installListeners) {
      if (!(aEvent in listener)) {
        continue;
      }
      try {
        listener[aEvent].apply(listener, aParams);
      } catch (e) {
        // this shouldn't be fatal
        Cu.reportError(e);
      }
    }
  },

  refreshGlobalWarning() {
    var page = document.getElementById("addons-page");

    if (Services.appinfo.inSafeMode) {
      page.setAttribute("warning", "safemode");
      return;
    }

    if (
      AddonManager.checkUpdateSecurityDefault &&
      !AddonManager.checkUpdateSecurity
    ) {
      page.setAttribute("warning", "updatesecurity");
      return;
    }

    if (!AddonManager.checkCompatibility) {
      page.setAttribute("warning", "checkcompatibility");
      return;
    }

    page.removeAttribute("warning");
  },

  refreshAutoUpdateDefault() {
    var updateEnabled = AddonManager.updateEnabled;
    var autoUpdateDefault = AddonManager.autoUpdateDefault;

    // The checkbox needs to reflect that both prefs need to be true
    // for updates to be checked for and applied automatically
    document
      .getElementById("utils-autoUpdateDefault")
      .setAttribute("checked", updateEnabled && autoUpdateDefault);

    document.getElementById(
      "utils-resetAddonUpdatesToAutomatic"
    ).hidden = !autoUpdateDefault;
    document.getElementById(
      "utils-resetAddonUpdatesToManual"
    ).hidden = autoUpdateDefault;
  },

  onCompatibilityModeChanged() {
    this.refreshGlobalWarning();
  },

  onCheckUpdateSecurityChanged() {
    this.refreshGlobalWarning();
  },

  onUpdateModeChanged() {
    this.refreshAutoUpdateDefault();
  },
};

var gViewController = {
  viewPort: null,
  currentViewId: "",
  currentViewObj: null,
  currentViewRequest: 0,
  viewObjects: {},
  viewChangeCallback: null,
  initialViewSelected: false,
  lastHistoryIndex: -1,
  backButton: null,

  initialize() {
    this.viewPort = document.getElementById("view-port");
    this.headeredViews = document.getElementById("headered-views");
    this.headeredViewsDeck = document.getElementById("headered-views-content");
    this.backButton = document.getElementById("go-back");

    this.viewObjects.legacy = gLegacyView;
    this.viewObjects.shortcuts = gShortcutsView;

    if (useHtmlViews) {
      this.viewObjects.list = htmlView("list");
      this.viewObjects.detail = htmlView("detail");
      this.viewObjects.updates = htmlView("updates");
      // gUpdatesView still handles when the Available Updates category is
      // shown. Include it in viewObjects so it gets initialized and shutdown.
      this.viewObjects._availableUpdatesSidebar = gUpdatesView;
    } else {
      this.viewObjects.list = gListView;
      this.viewObjects.detail = gDetailView;
      this.viewObjects.updates = gUpdatesView;
    }

    if (useHtmlDiscover && isDiscoverEnabled()) {
      this.viewObjects.discover = htmlView("discover");
    } else {
      this.viewObjects.discover = gDiscoverView;
    }

    for (let type in this.viewObjects) {
      let view = this.viewObjects[type];
      view.initialize();
    }

    window.controllers.appendController(this);

    window.addEventListener("popstate", function(e) {
      gViewController.updateState(e.state);
    });
  },

  shutdown() {
    if (this.currentViewObj) {
      this.currentViewObj.hide();
    }
    this.currentViewRequest = 0;

    for (let type in this.viewObjects) {
      let view = this.viewObjects[type];
      if ("shutdown" in view) {
        try {
          view.shutdown();
        } catch (e) {
          // this shouldn't be fatal
          Cu.reportError(e);
        }
      }
    }

    window.controllers.removeController(this);
  },

  updateState(state) {
    try {
      this.loadViewInternal(state.view, state.previousView, state);
      this.lastHistoryIndex = gHistory.index;
    } catch (e) {
      // The attempt to load the view failed, try moving further along history
      if (this.lastHistoryIndex > gHistory.index) {
        if (gHistory.canGoBack) {
          gHistory.back();
        } else {
          gViewController.replaceView(gViewDefault);
        }
      } else if (gHistory.canGoForward) {
        gHistory.forward();
      } else {
        gViewController.replaceView(gViewDefault);
      }
    }
  },

  parseViewId(aViewId) {
    var matchRegex = /^addons:\/\/([^\/]+)\/(.*)$/;
    var [, viewType, viewParam] = aViewId.match(matchRegex) || [];
    return { type: viewType, param: decodeURIComponent(viewParam) };
  },

  get isLoading() {
    return (
      !this.currentViewObj || this.currentViewObj.node.hasAttribute("loading")
    );
  },

  loadView(aViewId, sourceEvent) {
    var isRefresh = false;
    if (aViewId == this.currentViewId) {
      if (this.isLoading) {
        return;
      }
      if (!("refresh" in this.currentViewObj)) {
        return;
      }
      if (!this.currentViewObj.canRefresh()) {
        return;
      }
      isRefresh = true;
    }

    let isKeyboardNavigation =
      sourceEvent &&
      sourceEvent.mozInputSource === MouseEvent.MOZ_SOURCE_KEYBOARD;
    var state = {
      view: aViewId,
      previousView: this.currentViewId,
      isKeyboardNavigation,
    };
    if (!isRefresh) {
      gHistory.pushState(state);
      this.lastHistoryIndex = gHistory.index;
    }
    this.loadViewInternal(aViewId, this.currentViewId, state);
  },

  // Replaces the existing view with a new one, rewriting the current history
  // entry to match.
  replaceView(aViewId) {
    if (aViewId == this.currentViewId) {
      return;
    }

    var state = {
      view: aViewId,
      previousView: null,
    };
    gHistory.replaceState(state);
    this.loadViewInternal(aViewId, null, state);
  },

  loadInitialView(aViewId) {
    var state = {
      view: aViewId,
      previousView: null,
    };
    gHistory.replaceState(state);

    this.loadViewInternal(aViewId, null, state);
    notifyInitialized();
  },

  get displayedView() {
    if (this.viewPort.selectedPanel == this.headeredViews) {
      return this.headeredViewsDeck.selectedPanel;
    }
    return this.viewPort.selectedPanel;
  },

  set displayedView(view) {
    let node = view.node;
    if (node.parentNode == this.headeredViewsDeck) {
      this.headeredViewsDeck.selectedPanel = node;
      this.viewPort.selectedPanel = this.headeredViews;
    } else {
      this.viewPort.selectedPanel = node;
    }
  },

  loadViewInternal(aViewId, aPreviousView, aState, aEvent) {
    var view = this.parseViewId(aViewId);

    if (!view.type || !(view.type in this.viewObjects)) {
      throw Components.Exception("Invalid view: " + view.type);
    }

    var viewObj = this.viewObjects[view.type];
    if (!viewObj.node) {
      throw Components.Exception(
        "Root node doesn't exist for '" + view.type + "' view"
      );
    }

    if (this.currentViewObj && aViewId != aPreviousView) {
      try {
        let canHide = this.currentViewObj.hide();
        if (canHide === false) {
          return;
        }
        this.displayedView.removeAttribute("loading");
      } catch (e) {
        // this shouldn't be fatal
        Cu.reportError(e);
      }
    }

    gCategories.select(aViewId, aPreviousView);

    this.currentViewId = aViewId;
    this.currentViewObj = viewObj;

    this.displayedView = this.currentViewObj;
    this.currentViewObj.node.setAttribute("loading", "true");

    recordViewTelemetry(view.param);

    let headingName = document.getElementById("heading-name");
    let headingLabel;
    if (view.type == "discover") {
      headingLabel = gStrings.ext.formatStringFromName("listHeading.discover", [
        gStrings.brandShortName,
      ]);
    } else {
      try {
        headingLabel = gStrings.ext.GetStringFromName(
          `listHeading.${view.param}`
        );
      } catch (e) {
        // Some views don't have a label, like the updates view.
        headingLabel = "";
      }
    }
    headingName.textContent = headingLabel;
    setSearchLabel(view.param);

    if (aViewId == aPreviousView) {
      this.currentViewObj.refresh(
        view.param,
        ++this.currentViewRequest,
        aState
      );
    } else {
      this.currentViewObj.show(view.param, ++this.currentViewRequest, aState);
    }

    this.backButton.hidden = this.currentViewObj.isRoot || !gHistory.canGoBack;

    this.initialViewSelected = true;
  },

  // Moves back in the document history and removes the current history entry
  popState(aCallback) {
    this.viewChangeCallback = aCallback;
    gHistory.popState();
  },

  notifyViewChanged() {
    this.displayedView.removeAttribute("loading");

    if (this.viewChangeCallback) {
      this.viewChangeCallback();
      this.viewChangeCallback = null;
    }

    var event = document.createEvent("Events");
    event.initEvent("ViewChanged", true, true);
    this.currentViewObj.node.dispatchEvent(event);
  },

  commands: {
    cmd_back: {
      isEnabled() {
        return gHistory.canGoBack;
      },
      doCommand() {
        gHistory.back();
      },
    },

    cmd_forward: {
      isEnabled() {
        return gHistory.canGoForward;
      },
      doCommand() {
        gHistory.forward();
      },
    },

    cmd_focusSearch: {
      isEnabled: () => true,
      doCommand() {
        gHeader.focusSearchBox();
      },
    },

    cmd_enableCheckCompatibility: {
      isEnabled() {
        return true;
      },
      doCommand() {
        AddonManager.checkCompatibility = true;
      },
    },

    cmd_enableUpdateSecurity: {
      isEnabled() {
        return true;
      },
      doCommand() {
        AddonManager.checkUpdateSecurity = true;
      },
    },

    cmd_toggleAutoUpdateDefault: {
      isEnabled() {
        return true;
      },
      doCommand() {
        if (!AddonManager.updateEnabled || !AddonManager.autoUpdateDefault) {
          // One or both of the prefs is false, i.e. the checkbox is not checked.
          // Now toggle both to true. If the user wants us to auto-update
          // add-ons, we also need to auto-check for updates.
          AddonManager.updateEnabled = true;
          AddonManager.autoUpdateDefault = true;
        } else {
          // Both prefs are true, i.e. the checkbox is checked.
          // Toggle the auto pref to false, but don't touch the enabled check.
          AddonManager.autoUpdateDefault = false;
        }

        recordSetUpdatePolicyTelemetry();
      },
    },

    cmd_resetAddonAutoUpdate: {
      isEnabled() {
        return true;
      },
      async doCommand() {
        let aAddonList = await AddonManager.getAllAddons();
        for (let addon of aAddonList) {
          if ("applyBackgroundUpdates" in addon) {
            addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;
          }
        }
        recordActionTelemetry({ action: "resetUpdatePolicy" });
      },
    },

    cmd_goToDiscoverPane: {
      isEnabled() {
        return gDiscoverView.enabled;
      },
      doCommand() {
        gViewController.loadView("addons://discover/");
      },
    },

    cmd_goToRecentUpdates: {
      isEnabled() {
        return true;
      },
      doCommand() {
        gViewController.loadView("addons://updates/recent");
      },
    },

    cmd_goToAvailableUpdates: {
      isEnabled() {
        return true;
      },
      doCommand() {
        gViewController.loadView("addons://updates/available");
      },
    },

    cmd_showItemDetails: {
      isEnabled(aAddon) {
        return !!aAddon && gViewController.currentViewObj != gDetailView;
      },
      doCommand(aAddon, aScrollToPreferences) {
        gViewController.loadView(
          "addons://detail/" +
            encodeURIComponent(aAddon.id) +
            (aScrollToPreferences ? "/preferences" : "")
        );
      },
    },

    cmd_findAllUpdates: {
      inProgress: false,
      isEnabled() {
        return !this.inProgress;
      },
      async doCommand() {
        this.inProgress = true;
        gViewController.updateCommand("cmd_findAllUpdates");
        document.getElementById("updates-noneFound").hidden = true;
        document.getElementById("updates-progress").hidden = false;
        document.getElementById("updates-manualUpdatesFound-btn").hidden = true;

        var pendingChecks = 0;
        var numUpdated = 0;
        var numManualUpdates = 0;

        let updateStatus = () => {
          if (pendingChecks > 0) {
            return;
          }

          this.inProgress = false;
          gViewController.updateCommand("cmd_findAllUpdates");
          document.getElementById("updates-progress").hidden = true;
          gUpdatesView.maybeRefresh();

          Services.obs.notifyObservers(null, "EM-update-check-finished");

          if (numManualUpdates > 0 && numUpdated == 0) {
            document.getElementById(
              "updates-manualUpdatesFound-btn"
            ).hidden = false;
            return;
          }

          if (numUpdated == 0) {
            document.getElementById("updates-noneFound").hidden = false;
            return;
          }

          document.getElementById("updates-installed").hidden = false;
        };

        var updateInstallListener = {
          onDownloadFailed() {
            pendingChecks--;
            updateStatus();
          },
          onInstallCancelled() {
            pendingChecks--;
            updateStatus();
          },
          onInstallFailed() {
            pendingChecks--;
            updateStatus();
          },
          onInstallEnded(aInstall, aAddon) {
            pendingChecks--;
            numUpdated++;
            updateStatus();
          },
        };

        var updateCheckListener = {
          onUpdateAvailable(aAddon, aInstall) {
            gEventManager.delegateAddonEvent("onUpdateAvailable", [
              aAddon,
              aInstall,
            ]);
            attachUpdateHandler(aInstall);
            if (AddonManager.shouldAutoUpdate(aAddon)) {
              aInstall.addListener(updateInstallListener);
              aInstall.install();
            } else {
              pendingChecks--;
              numManualUpdates++;
              updateStatus();
            }
          },
          onNoUpdateAvailable(aAddon) {
            pendingChecks--;
            updateStatus();
          },
          onUpdateFinished(aAddon, aError) {
            gEventManager.delegateAddonEvent("onUpdateFinished", [
              aAddon,
              aError,
            ]);
          },
        };

        let aAddonList = await AddonManager.getAddonsByTypes(null);
        for (let addon of aAddonList) {
          if (addon.permissions & AddonManager.PERM_CAN_UPGRADE) {
            pendingChecks++;
            addon.findUpdates(
              updateCheckListener,
              AddonManager.UPDATE_WHEN_USER_REQUESTED
            );
          }
        }

        recordActionTelemetry({ action: "checkForUpdates" });

        if (pendingChecks == 0) {
          updateStatus();
        }
      },
    },

    cmd_findItemUpdates: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        return hasPermission(aAddon, "upgrade");
      },
      doCommand(aAddon) {
        var listener = {
          onUpdateAvailable(aAddon, aInstall) {
            gEventManager.delegateAddonEvent("onUpdateAvailable", [
              aAddon,
              aInstall,
            ]);
            attachUpdateHandler(aInstall);
            if (AddonManager.shouldAutoUpdate(aAddon)) {
              aInstall.install();
            }
          },
          onNoUpdateAvailable(aAddon) {
            gEventManager.delegateAddonEvent("onNoUpdateAvailable", [aAddon]);
          },
        };
        gEventManager.delegateAddonEvent("onCheckingUpdate", [aAddon]);
        aAddon.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
        recordActionTelemetry({ action: "checkForUpdate", addon: aAddon });
      },
    },

    cmd_showItemPreferences: {
      isEnabled(aAddon) {
        if (!aAddon || (!aAddon.isActive && aAddon.type !== "plugin")) {
          return false;
        }
        if (gViewController.currentViewObj == gDetailView) {
          return aAddon.optionsType && !hasInlineOptions(aAddon);
        }
        return aAddon.type == "plugin" || aAddon.optionsType;
      },
      doCommand(aAddon) {
        let inline = hasInlineOptions(aAddon);
        let view = getCurrentViewName();

        if (inline) {
          gViewController.commands.cmd_showItemDetails.doCommand(aAddon, true);
        } else if (aAddon.optionsType == AddonManager.OPTIONS_TYPE_TAB) {
          openOptionsInTab(aAddon.optionsURL);
        }

        let value = inline ? "inline" : "external";
        recordActionTelemetry({
          action: "preferences",
          value,
          view,
          addon: aAddon,
        });
      },
    },

    cmd_enableItem: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        let addonType = AddonManager.addonTypes[aAddon.type];
        return (
          !(addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) &&
          hasPermission(aAddon, "enable")
        );
      },
      doCommand(aAddon) {
        if (shouldShowPermissionsPrompt(aAddon)) {
          showPermissionsPrompt(aAddon).then(() => {
            // Record telemetry if the addon has been enabled.
            recordActionTelemetry({ action: "enable", addon: aAddon });
          });
        } else {
          aAddon.enable();
          recordActionTelemetry({ action: "enable", addon: aAddon });
        }
      },
      getTooltip(aAddon) {
        if (!aAddon) {
          return "";
        }
        return gStrings.ext.GetStringFromName("enableAddonTooltip");
      },
    },

    cmd_disableItem: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        let addonType = AddonManager.addonTypes[aAddon.type];
        return (
          !(addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) &&
          hasPermission(aAddon, "disable")
        );
      },
      doCommand(aAddon) {
        aAddon.disable();
        recordActionTelemetry({ action: "disable", addon: aAddon });
      },
      getTooltip(aAddon) {
        if (!aAddon) {
          return "";
        }
        return gStrings.ext.GetStringFromName("disableAddonTooltip");
      },
    },

    cmd_installItem: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        return (
          aAddon.install && aAddon.install.state == AddonManager.STATE_AVAILABLE
        );
      },
      doCommand(aAddon) {
        function doInstall() {
          gViewController.currentViewObj
            .getListItemForID(aAddon.id)
            ._installStatus.installRemote();
        }

        if (gViewController.currentViewObj == gDetailView) {
          gViewController.popState(doInstall);
        } else {
          doInstall();
        }
      },
    },

    cmd_uninstallItem: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        return hasPermission(aAddon, "uninstall");
      },
      async doCommand(aAddon) {
        let view = getCurrentViewName();

        // Make sure we're on the list view, which supports undo.
        if (gViewController.currentViewObj != gListView) {
          await new Promise(resolve => {
            document.addEventListener("ViewChanged", resolve, { once: true });
            gViewController.loadView(`addons://list/${aAddon.type}`);
          });
        }
        recordActionTelemetry({ action: "uninstall", view, addon: aAddon });
        gViewController.currentViewObj.getListItemForID(aAddon.id).uninstall();
      },
      getTooltip(aAddon) {
        if (!aAddon) {
          return "";
        }
        return gStrings.ext.GetStringFromName("uninstallAddonTooltip");
      },
    },

    cmd_cancelUninstallItem: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        return isPending(aAddon, "uninstall");
      },
      doCommand(aAddon) {
        aAddon.cancelUninstall();
      },
    },

    cmd_installFromFile: {
      isEnabled() {
        return XPINSTALL_ENABLED;
      },
      doCommand() {
        const nsIFilePicker = Ci.nsIFilePicker;
        var fp = Cc["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
        fp.init(
          window,
          gStrings.ext.GetStringFromName("installFromFile.dialogTitle"),
          nsIFilePicker.modeOpenMultiple
        );
        try {
          fp.appendFilter(
            gStrings.ext.GetStringFromName("installFromFile.filterName"),
            "*.xpi;*.jar;*.zip"
          );
          fp.appendFilters(nsIFilePicker.filterAll);
        } catch (e) {}

        fp.open(async result => {
          if (result != nsIFilePicker.returnOK) {
            return;
          }

          let installTelemetryInfo = {
            source: "about:addons",
            method: "install-from-file",
          };

          let browser = getBrowserElement();
          for (let file of fp.files) {
            let install = await AddonManager.getInstallForFile(
              file,
              null,
              installTelemetryInfo
            );
            AddonManager.installAddonFromAOM(
              browser,
              document.documentURIObject,
              install
            );
            recordActionTelemetry({
              action: "installFromFile",
              value: install.installId,
            });
          }
        });
      },
    },

    cmd_debugAddons: {
      isEnabled() {
        return true;
      },
      doCommand() {
        let mainWindow = window.windowRoot.ownerGlobal;
        recordLinkTelemetry("about:debugging");
        if ("switchToTabHavingURI" in mainWindow) {
          let path = useNewAboutDebugging ? "/runtime/this-firefox" : "addons";
          mainWindow.switchToTabHavingURI(`about:debugging#${path}`, true, {
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          });
        }
      },
    },

    cmd_cancelOperation: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        return aAddon.pendingOperations != AddonManager.PENDING_NONE;
      },
      doCommand(aAddon) {
        if (isPending(aAddon, "uninstall")) {
          aAddon.cancelUninstall();
        }
      },
    },

    cmd_contribute: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        return "contributionURL" in aAddon && aAddon.contributionURL;
      },
      doCommand(aAddon) {
        openURL(aAddon.contributionURL);
        recordActionTelemetry({ action: "contribute", addon: aAddon });
      },
    },

    cmd_askToActivateItem: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        let addonType = AddonManager.addonTypes[aAddon.type];
        return (
          addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE &&
          hasPermission(aAddon, "ask_to_activate")
        );
      },
      doCommand(aAddon) {
        aAddon.userDisabled = AddonManager.STATE_ASK_TO_ACTIVATE;
      },
    },

    cmd_alwaysActivateItem: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        let addonType = AddonManager.addonTypes[aAddon.type];
        return (
          addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE &&
          hasPermission(aAddon, "enable")
        );
      },
      doCommand(aAddon) {
        aAddon.userDisabled = false;
      },
    },

    cmd_neverActivateItem: {
      isEnabled(aAddon) {
        if (!aAddon) {
          return false;
        }
        let addonType = AddonManager.addonTypes[aAddon.type];
        return (
          addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE &&
          hasPermission(aAddon, "disable")
        );
      },
      doCommand(aAddon) {
        aAddon.userDisabled = true;
      },
    },

    cmd_showUnsignedExtensions: {
      isEnabled() {
        return true;
      },
      doCommand() {
        gViewController.loadView("addons://list/extension?unsigned=true");
      },
    },

    cmd_showAllExtensions: {
      isEnabled() {
        return true;
      },
      doCommand() {
        gViewController.loadView("addons://list/extension");
      },
    },

    cmd_showShortcuts: {
      isEnabled() {
        return true;
      },
      doCommand() {
        gViewController.loadView("addons://shortcuts/shortcuts");
      },
    },
  },

  supportsCommand(aCommand) {
    return aCommand in this.commands;
  },

  isCommandEnabled(aCommand) {
    if (!this.supportsCommand(aCommand)) {
      return false;
    }
    var addon = this.currentViewObj.getSelectedAddon();
    return this.commands[aCommand].isEnabled(addon);
  },

  updateCommands() {
    // wait until the view is initialized
    if (!this.currentViewObj) {
      return;
    }
    var addon = this.currentViewObj.getSelectedAddon();
    for (let commandId in this.commands) {
      this.updateCommand(commandId, addon);
    }
  },

  updateCommand(aCommandId, aAddon) {
    if (typeof aAddon == "undefined") {
      aAddon = this.currentViewObj.getSelectedAddon();
    }
    var cmd = this.commands[aCommandId];
    var cmdElt = document.getElementById(aCommandId);
    cmdElt.setAttribute("disabled", !cmd.isEnabled(aAddon));
    if ("getTooltip" in cmd) {
      let tooltip = cmd.getTooltip(aAddon);
      if (tooltip) {
        cmdElt.setAttribute("tooltiptext", tooltip);
      } else {
        cmdElt.removeAttribute("tooltiptext");
      }
    }
  },

  doCommand(aCommand, aAddon) {
    if (!this.supportsCommand(aCommand)) {
      return;
    }
    var cmd = this.commands[aCommand];
    if (!aAddon) {
      aAddon = this.currentViewObj.getSelectedAddon();
    }
    if (!cmd.isEnabled(aAddon)) {
      return;
    }
    cmd.doCommand(aAddon);
  },

  onEvent() {},
};

async function isAddonAllowedInCurrentWindow(aAddon) {
  if (
    allowPrivateBrowsingByDefault ||
    aAddon.type !== "extension" ||
    !PrivateBrowsingUtils.isContentWindowPrivate(window)
  ) {
    return true;
  }

  const perms = await ExtensionPermissions.get(aAddon.id);
  return perms.permissions.includes("internal:privateBrowsingAllowed");
}

function hasInlineOptions(aAddon) {
  return (
    aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE_BROWSER ||
    aAddon.type == "plugin"
  );
}

function formatDate(aDate) {
  const dtOptions = { year: "numeric", month: "long", day: "numeric" };
  return aDate.toLocaleDateString(undefined, dtOptions);
}

function hasPermission(aAddon, aPerm) {
  var perm = AddonManager["PERM_CAN_" + aPerm.toUpperCase()];
  return !!(aAddon.permissions & perm);
}

function isPending(aAddon, aAction) {
  var action = AddonManager["PENDING_" + aAction.toUpperCase()];
  return !!(aAddon.pendingOperations & action);
}

function isInState(aInstall, aState) {
  var state = AddonManager["STATE_" + aState.toUpperCase()];
  return aInstall.state == state;
}

function shouldShowVersionNumber(aAddon) {
  if (!aAddon.version) {
    return false;
  }

  // The version number is hidden for lightweight themes.
  if (aAddon.type == "theme") {
    return !/@personas\.mozilla\.org$/.test(aAddon.id);
  }

  return true;
}

function createItem(aObj, aIsInstall) {
  let item = document.createXULElement("richlistitem");

  item.setAttribute("class", "addon addon-view card");
  item.setAttribute("name", aObj.name);
  item.setAttribute("type", aObj.type);

  if (aIsInstall) {
    item.mInstall = aObj;

    if (aObj.state != AddonManager.STATE_INSTALLED) {
      item.setAttribute("status", "installing");
      return item;
    }
    aObj = aObj.addon;
  }

  item.mAddon = aObj;

  item.setAttribute("status", "installed");

  // set only attributes needed for sorting and XBL binding,
  // the binding handles the rest
  item.setAttribute("value", aObj.id);

  return item;
}

function sortElements(aElements, aSortBy, aAscending) {
  // aSortBy is an Array of attributes to sort by, in decending
  // order of priority.

  const DATE_FIELDS = ["updateDate"];
  const NUMERIC_FIELDS = ["relevancescore"];

  // We're going to group add-ons into the following buckets:
  //
  //  enabledInstalled
  //    * Enabled
  //    * Incompatible but enabled because compatibility checking is off
  //    * Waiting to be installed
  //    * Waiting to be enabled
  //
  //  pendingDisable
  //    * Waiting to be disabled
  //
  //  pendingUninstall
  //    * Waiting to be removed
  //
  //  disabledIncompatibleBlocked
  //    * Disabled
  //    * Incompatible
  //    * Blocklisted

  const UISTATE_ORDER = [
    "enabled",
    "askToActivate",
    "pendingDisable",
    "pendingUninstall",
    "disabled",
  ];

  function dateCompare(a, b) {
    var aTime = a.getTime();
    var bTime = b.getTime();
    if (aTime < bTime) {
      return -1;
    }
    if (aTime > bTime) {
      return 1;
    }
    return 0;
  }

  function numberCompare(a, b) {
    return a - b;
  }

  function stringCompare(a, b) {
    return a.localeCompare(b);
  }

  function uiStateCompare(a, b) {
    // If we're in descending order, swap a and b, because
    // we don't ever want to have descending uiStates
    if (!aAscending) {
      [a, b] = [b, a];
    }

    return UISTATE_ORDER.indexOf(a) - UISTATE_ORDER.indexOf(b);
  }

  // Prioritize themes that have screenshots.
  function hasPreview(aHasStr, bHasStr) {
    let aHas = aHasStr == "true";
    let bHas = bHasStr == "true";
    if (aHas == bHas) {
      return 0;
    }
    return aHas ? -1 : 1;
  }

  function getValue(aObj, aKey) {
    if (!aObj) {
      return null;
    }

    if (aObj.hasAttribute(aKey)) {
      return aObj.getAttribute(aKey);
    }

    var addon = aObj.mAddon || aObj.mInstall;
    var addonType = aObj.mAddon && AddonManager.addonTypes[aObj.mAddon.type];

    if (!addon) {
      return null;
    }

    if (aKey == "uiState") {
      if (addon.pendingOperations == AddonManager.PENDING_DISABLE) {
        return "pendingDisable";
      }
      if (addon.pendingOperations == AddonManager.PENDING_UNINSTALL) {
        return "pendingUninstall";
      }
      if (
        !addon.isActive &&
        (addon.pendingOperations != AddonManager.PENDING_ENABLE &&
          addon.pendingOperations != AddonManager.PENDING_INSTALL)
      ) {
        return "disabled";
      }
      if (
        addonType &&
        addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE &&
        addon.userDisabled == AddonManager.STATE_ASK_TO_ACTIVATE
      ) {
        return "askToActivate";
      }
      return "enabled";
    }

    return addon[aKey];
  }

  // aSortFuncs will hold the sorting functions that we'll
  // use per element, in the correct order.
  var aSortFuncs = [];

  for (let i = 0; i < aSortBy.length; i++) {
    var sortBy = aSortBy[i];

    aSortFuncs[i] = stringCompare;

    if (sortBy == "uiState") {
      aSortFuncs[i] = uiStateCompare;
    } else if (DATE_FIELDS.includes(sortBy)) {
      aSortFuncs[i] = dateCompare;
    } else if (NUMERIC_FIELDS.includes(sortBy)) {
      aSortFuncs[i] = numberCompare;
    } else if (sortBy == "hasPreview") {
      aSortFuncs[i] = hasPreview;
    }
  }

  aElements.sort(function(a, b) {
    if (!aAscending) {
      [a, b] = [b, a];
    }

    for (let i = 0; i < aSortFuncs.length; i++) {
      var sortBy = aSortBy[i];
      var aValue = getValue(a, sortBy);
      var bValue = getValue(b, sortBy);

      if (!aValue && !bValue) {
        return 0;
      }
      if (!aValue) {
        return -1;
      }
      if (!bValue) {
        return 1;
      }
      if (aValue != bValue) {
        var result = aSortFuncs[i](aValue, bValue);

        if (result != 0) {
          return result;
        }
      }
    }

    // If we got here, then all values of a and b
    // must have been equal.
    return 0;
  });
}

function sortList(aList, aSortBy, aAscending) {
  var elements = Array.from(aList.childNodes);
  sortElements(elements, [aSortBy], aAscending);

  while (aList.lastChild) {
    aList.lastChild.remove();
  }

  for (let element of elements) {
    aList.appendChild(element);
  }
}

async function getAddonsAndInstalls(aType, aCallback) {
  let addons = null,
    installs = null;
  let types = aType != null ? [aType] : null;

  let aAddonsList = await AddonManager.getAddonsByTypes(types);
  addons = aAddonsList.filter(a => !a.hidden);
  if (installs != null) {
    aCallback(addons, installs);
  }

  let aInstallsList = await AddonManager.getInstallsByTypes(types);
  installs = aInstallsList.filter(function(aInstall) {
    return !(
      aInstall.existingAddon || aInstall.state == AddonManager.STATE_AVAILABLE
    );
  });

  if (addons != null) {
    aCallback(addons, installs);
  }
}

function doPendingUninstalls(aListBox) {
  // Uninstalling add-ons can mutate the list so find the add-ons first then
  // uninstall them
  var items = [];
  var listitem = aListBox.firstChild;
  while (listitem) {
    if (listitem.getAttribute("pending") == "uninstall") {
      items.push(listitem.mAddon);
    }
    listitem = listitem.nextSibling;
  }

  for (let addon of items) {
    addon.uninstall();
  }
}

var gCategories = {
  node: null,

  initialize() {
    this.node = document.getElementById("categories");

    var types = AddonManager.addonTypes;
    for (var type in types) {
      this.onTypeAdded(types[type]);
    }

    AddonManager.addTypeListener(this);

    let lastView = Services.prefs.getStringPref(PREF_UI_LASTCATEGORY, "");
    // Set this to the default value first, or setting it to a nonexistent value
    // from the pref will leave the old value in place.
    this.node.value = gViewDefault;
    this.node.value = lastView;
    // Fixup the last view if legacy is disabled.
    if (lastView !== this.node.value && lastView == "addons://legacy/") {
      this.node.value = "addons://list/extension";
    }
    // If there was no last view or no existing category matched the last view
    // then switch to the default category
    if (!this.node.selectedItem) {
      this.node.value = gViewDefault;
    }
    // If the previous node is the discover panel which has since been disabled set to default
    if (this.node.value == "addons://discover/" && !isDiscoverEnabled()) {
      this.node.value = gViewDefault;
    }

    this.node.addEventListener("select", () => {
      gViewController.loadView(this.node.selectedItem.value);
    });

    this.node.addEventListener("click", aEvent => {
      var selectedItem = this.node.selectedItem;
      if (
        aEvent.target.localName == "richlistitem" &&
        aEvent.target == selectedItem
      ) {
        var viewId = selectedItem.value;

        gViewController.loadView(viewId);
      }
    });
  },

  shutdown() {
    AddonManager.removeTypeListener(this);
  },

  _insertCategory(aId, aName, aView, aPriority, aStartHidden) {
    // If this category already exists then don't re-add it
    if (document.getElementById("category-" + aId)) {
      return;
    }

    var category = document.createXULElement("richlistitem");
    category.setAttribute("id", "category-" + aId);
    category.setAttribute("value", aView);
    category.setAttribute("class", "category");
    category.setAttribute("name", aName);
    category.setAttribute("tooltiptext", aName);
    category.setAttribute("priority", aPriority);
    category.setAttribute("hidden", aStartHidden);

    var node;
    for (node of this.node.itemChildren) {
      var nodePriority = parseInt(node.getAttribute("priority"));
      // If the new type's priority is higher than this one then this is the
      // insertion point
      if (aPriority < nodePriority) {
        break;
      }
      // If the new type's priority is lower than this one then this is isn't
      // the insertion point
      if (aPriority > nodePriority) {
        continue;
      }
      // If the priorities are equal and the new type's name is earlier
      // alphabetically then this is the insertion point
      if (String(aName).localeCompare(node.getAttribute("name")) < 0) {
        break;
      }
    }

    this.node.insertBefore(category, node);
  },

  _removeCategory(aId) {
    var category = document.getElementById("category-" + aId);
    if (!category) {
      return;
    }

    // If this category is currently selected then switch to the default view
    if (this.node.selectedItem == category) {
      gViewController.replaceView(gViewDefault);
    }

    this.node.removeChild(category);
  },

  onTypeAdded(aType) {
    // Ignore types that we don't have a view object for
    if (!(aType.viewType in gViewController.viewObjects)) {
      return;
    }

    var aViewId = "addons://" + aType.viewType + "/" + aType.id;

    var startHidden = false;
    if (aType.flags & AddonManager.TYPE_UI_HIDE_EMPTY) {
      var prefName = PREF_UI_TYPE_HIDDEN.replace("%TYPE%", aType.id);
      startHidden = Services.prefs.getBoolPref(prefName, true);

      gPendingInitializations++;
      getAddonsAndInstalls(aType.id, (aAddonsList, aInstallsList) => {
        var hidden = aAddonsList.length == 0 && aInstallsList.length == 0;
        var item = this.get(aViewId);

        // Don't load view that is becoming hidden
        if (hidden && aViewId == gViewController.currentViewId) {
          gViewController.loadView(gViewDefault);
        }

        item.hidden = hidden;
        Services.prefs.setBoolPref(prefName, hidden);

        if (aAddonsList.length > 0 || aInstallsList.length > 0) {
          notifyInitialized();
          return;
        }

        gEventManager.registerInstallListener({
          onDownloadStarted(aInstall) {
            this._maybeShowCategory(aInstall);
          },

          onInstallStarted(aInstall) {
            this._maybeShowCategory(aInstall);
          },

          onInstallEnded(aInstall, aAddon) {
            this._maybeShowCategory(aAddon);
          },

          onExternalInstall(aAddon, aExistingAddon) {
            this._maybeShowCategory(aAddon);
          },

          _maybeShowCategory: aAddon => {
            if (aType.id == aAddon.type) {
              this.get(aViewId).hidden = false;
              Services.prefs.setBoolPref(prefName, false);
              gEventManager.unregisterInstallListener(this);
            }
          },
        });

        notifyInitialized();
      });
    }

    this._insertCategory(
      aType.id,
      aType.name,
      aViewId,
      aType.uiPriority,
      startHidden
    );
  },

  onTypeRemoved(aType) {
    this._removeCategory(aType.id);
  },

  get selected() {
    return this.node.selectedItem ? this.node.selectedItem.value : null;
  },

  select(aId, aPreviousView) {
    var view = gViewController.parseViewId(aId);
    if (view.type == "detail" && aPreviousView) {
      aId = aPreviousView;
      view = gViewController.parseViewId(aPreviousView);
    }
    aId = aId.replace(/\?.*/, "");

    Services.prefs.setCharPref(PREF_UI_LASTCATEGORY, aId);

    if (this.node.selectedItem && this.node.selectedItem.value == aId) {
      this.node.selectedItem.hidden = false;
      this.node.selectedItem.disabled = false;
      return;
    }

    var item = this.get(aId);

    if (item) {
      item.hidden = false;
      item.disabled = false;
      this.node.suppressOnSelect = true;
      this.node.selectedItem = item;
      this.node.suppressOnSelect = false;
      this.node.ensureElementIsVisible(item);
    }
  },

  get(aId) {
    var items = document.getElementsByAttribute("value", aId);
    if (items.length) {
      return items[0];
    }
    return null;
  },

  setBadge(aId, aCount) {
    let item = this.get(aId);
    if (item) {
      item.badgeCount = aCount;
    }
  },
};

var gHeader = {
  _search: null,
  _dest: "",

  initialize() {
    this._search = document.getElementById("header-search");

    this._search.addEventListener("command", function(aEvent) {
      var query = aEvent.target.value;
      if (query.length == 0) {
        return;
      }

      let url = AddonRepository.getSearchURL(query);

      let browser = getBrowserElement();
      let chromewin = browser.ownerGlobal;
      chromewin.openLinkIn(url, "tab", {
        fromChrome: true,
        triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
          {}
        ),
      });

      recordLinkTelemetry("search");
    });
  },

  focusSearchBox() {
    this._search.focus();
  },

  onKeyPress(aEvent) {
    if (String.fromCharCode(aEvent.charCode) == "/") {
      this.focusSearchBox();
    }
  },

  get shouldShowNavButtons() {
    var docshellItem = window.docShell;

    // If there is no outer frame then make the buttons visible
    if (docshellItem.rootTreeItem == docshellItem) {
      return true;
    }

    var outerWin = docshellItem.rootTreeItem.domWindow;
    var outerDoc = outerWin.document;
    var node = outerDoc.getElementById("back-button");
    // If the outer frame has no back-button then make the buttons visible
    if (!node) {
      return true;
    }

    // If the back-button or any of its parents are hidden then make the buttons
    // visible
    while (node != outerDoc) {
      var style = outerWin.getComputedStyle(node);
      if (style.display == "none") {
        return true;
      }
      if (style.visibility != "visible") {
        return true;
      }
      node = node.parentNode;
    }

    return false;
  },

  get searchQuery() {
    return this._search.value;
  },

  set searchQuery(aQuery) {
    this._search.value = aQuery;
  },
};

var gDiscoverView = {
  node: null,
  enabled: true,
  // Set to true after the view is first shown. If initialization completes
  // after this then it must also load the discover homepage
  loaded: false,
  _browser: null,
  _loading: null,
  _error: null,
  homepageURL: null,
  _loadListeners: [],
  hideHeader: true,
  isRoot: true,

  get clientIdDiscoveryEnabled() {
    // These prefs match Discovery.jsm for enabling clientId cookies.
    return (
      Services.prefs.getBoolPref(
        "datareporting.healthreport.uploadEnabled",
        false
      ) &&
      Services.prefs.getBoolPref("browser.discovery.enabled", false) &&
      !PrivateBrowsingUtils.isContentWindowPrivate(window)
    );
  },

  async getClientHeader() {
    if (!this.clientIdDiscoveryEnabled) {
      return undefined;
    }
    let clientId = await ClientID.getClientIdHash();

    let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
      Ci.nsISupportsCString
    );
    stream.data = `Moz-Client-Id: ${clientId}\r\n`;
    return stream;
  },

  async initialize() {
    this.enabled = isDiscoverEnabled();
    if (!this.enabled) {
      gCategories.get("addons://discover/").hidden = true;
      return null;
    }

    this.node = document.getElementById("discover-view");
    this._loading = document.getElementById("discover-loading");
    this._error = document.getElementById("discover-error");
    this._browser = document.getElementById("discover-browser");

    let compatMode = "normal";
    if (!AddonManager.checkCompatibility) {
      compatMode = "ignore";
    } else if (AddonManager.strictCompatibility) {
      compatMode = "strict";
    }

    var url = Services.prefs.getCharPref(PREF_DISCOVERURL);
    url = url.replace("%COMPATIBILITY_MODE%", compatMode);
    url = Services.urlFormatter.formatURL(url);

    let setURL = async aURL => {
      try {
        this.homepageURL = Services.io.newURI(aURL);
      } catch (e) {
        this.showError();
        notifyInitialized();
        return;
      }

      this._browser.addProgressListener(
        this,
        Ci.nsIWebProgress.NOTIFY_STATE_ALL |
          Ci.nsIWebProgress.NOTIFY_SECURITY |
          Ci.nsIWebProgress.NOTIFY_LOCATION
      );

      if (this.loaded) {
        this._loadURL(
          this.homepageURL.spec,
          false,
          notifyInitialized,
          Services.scriptSecurityManager.getSystemPrincipal(),
          await this.getClientHeader()
        );
      } else {
        notifyInitialized();
      }
    };

    if (!Services.prefs.getBoolPref(PREF_GETADDONS_CACHE_ENABLED)) {
      return setURL(url);
    }

    gPendingInitializations++;
    let aAddons = await AddonManager.getAddonsByTypes(["extension", "theme"]);
    var list = {};
    for (let addon of aAddons) {
      var prefName = PREF_GETADDONS_CACHE_ID_ENABLED.replace("%ID%", addon.id);
      try {
        if (!Services.prefs.getBoolPref(prefName)) {
          continue;
        }
      } catch (e) {}
      list[addon.id] = {
        name: addon.name,
        version: addon.version,
        type: addon.type,
        userDisabled: addon.userDisabled,
        isCompatible: addon.isCompatible,
        isBlocklisted:
          addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED,
      };
    }

    return setURL(url + "#" + JSON.stringify(list));
  },

  destroy() {
    try {
      this._browser.removeProgressListener(this);
    } catch (e) {
      // Ignore the case when the listener wasn't already registered
    }
  },

  async show(aParam, aRequest, aState, aIsRefresh) {
    gViewController.updateCommands();

    // If we're being told to load a specific URL then just do that
    if (aState && "url" in aState) {
      this.loaded = true;
      this._loadURL(aState.url);
    }

    // If the view has loaded before and still at the homepage (if refreshing),
    // and the error page is not visible then there is nothing else to do
    if (
      this.loaded &&
      this.node.selectedPanel != this._error &&
      (!aIsRefresh ||
        (this._browser.currentURI &&
          this._browser.currentURI.spec == this.homepageURL.spec))
    ) {
      gViewController.notifyViewChanged();
      return;
    }

    this.loaded = true;

    // No homepage means initialization isn't complete, the browser will get
    // loaded once initialization is complete
    if (!this.homepageURL) {
      this._loadListeners.push(
        gViewController.notifyViewChanged.bind(gViewController)
      );
      return;
    }

    this._loadURL(
      this.homepageURL.spec,
      aIsRefresh,
      gViewController.notifyViewChanged.bind(gViewController),
      Services.scriptSecurityManager.getSystemPrincipal(),
      await this.getClientHeader()
    );
  },

  canRefresh() {
    if (
      this._browser.currentURI &&
      this._browser.currentURI.spec == this.homepageURL.spec
    ) {
      return false;
    }
    return true;
  },

  refresh(aParam, aRequest, aState) {
    this.show(aParam, aRequest, aState, true);
  },

  hide() {},

  showError() {
    this.node.selectedPanel = this._error;
  },

  _loadURL(aURL, aKeepHistory, aCallback, aPrincipal, headers) {
    if (this._browser.currentURI && this._browser.currentURI.spec == aURL) {
      if (aCallback) {
        aCallback();
      }
      return;
    }

    if (aCallback) {
      this._loadListeners.push(aCallback);
    }

    var flags = 0;
    if (!aKeepHistory) {
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
    }

    this._browser.loadURI(aURL, {
      flags,
      triggeringPrincipal:
        aPrincipal || Services.scriptSecurityManager.createNullPrincipal({}),
      headers,
    });
  },

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    // Ignore the about:blank load
    if (aLocation.spec == "about:blank") {
      return;
    }

    // When using the real session history the inner-frame will update the
    // session history automatically, if using the fake history though it must
    // be manually updated
    if (gHistory == FakeHistory) {
      var docshell = aWebProgress.QueryInterface(Ci.nsIDocShell);

      var state = {
        view: "addons://discover/",
        url: aLocation.spec,
      };

      var replaceHistory = Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY << 16;
      if (docshell.loadType & replaceHistory) {
        gHistory.replaceState(state);
      } else {
        gHistory.pushState(state);
      }
      gViewController.lastHistoryIndex = gHistory.index;
    }

    gViewController.updateCommands();

    // If the hostname is the same as the new location's host and either the
    // default scheme is insecure or the new location is secure then continue
    // with the load
    if (
      aLocation.host == this.homepageURL.host &&
      (!this.homepageURL.schemeIs("https") || aLocation.schemeIs("https"))
    ) {
      return;
    }

    // Canceling the request will send an error to onStateChange which will show
    // the error page
    aRequest.cancel(Cr.NS_BINDING_ABORTED);
  },

  onSecurityChange(aWebProgress, aRequest, aState) {
    // Don't care about security if the page is not https
    if (!this.homepageURL.schemeIs("https")) {
      return;
    }

    // If the request was secure then it is ok
    if (aState & Ci.nsIWebProgressListener.STATE_IS_SECURE) {
      return;
    }

    // Canceling the request will send an error to onStateChange which will show
    // the error page
    aRequest.cancel(Cr.NS_BINDING_ABORTED);
  },

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let transferStart =
      Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
      Ci.nsIWebProgressListener.STATE_IS_REQUEST |
      Ci.nsIWebProgressListener.STATE_TRANSFERRING;
    // Once transferring begins show the content
    if ((aStateFlags & transferStart) === transferStart) {
      this.node.selectedPanel = this._browser;
    }

    // Only care about the network events
    if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK)) {
      return;
    }

    // If this is the start of network activity then show the loading page
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      this.node.selectedPanel = this._loading;
    }

    // Ignore anything except stop events
    if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)) {
      return;
    }

    // Consider the successful load of about:blank as still loading
    if (
      aRequest instanceof Ci.nsIChannel &&
      aRequest.URI.spec == "about:blank"
    ) {
      return;
    }

    // If there was an error loading the page or the new hostname is not the
    // same as the default hostname or the default scheme is secure and the new
    // scheme is insecure then show the error page
    const NS_ERROR_PARSED_DATA_CACHED = 0x805d0021;
    if (
      !(
        Components.isSuccessCode(aStatus) ||
        aStatus == NS_ERROR_PARSED_DATA_CACHED
      ) ||
      (aRequest &&
        aRequest instanceof Ci.nsIHttpChannel &&
        !aRequest.requestSucceeded)
    ) {
      this.showError();
    } else {
      // Got a successful load, make sure the browser is visible
      this.node.selectedPanel = this._browser;
      gViewController.updateCommands();
    }

    var listeners = this._loadListeners;
    this._loadListeners = [];

    for (let listener of listeners) {
      listener();
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
  ]),

  getSelectedAddon() {
    return null;
  },
};

var gLegacyView = {
  node: null,
  _listBox: null,
  _categoryItem: null,
  isRoot: true,

  initialize() {
    this.node = document.getElementById("legacy-view");
    this._listBox = document.getElementById("legacy-list");
    this._categoryItem = gCategories.get("addons://legacy/");

    document.getElementById("legacy-learnmore").href =
      SUPPORT_URL + "webextensions";

    gEventManager.registerAddonListener(this, "ANY");

    this.refreshVisibility();
  },

  shutdown() {
    gEventManager.unregisterAddonListener(this, "ANY");
  },

  onUninstalled() {
    this.refreshVisibility();
  },

  async show(type, request) {
    let addons = await AddonManager.getAddonsByTypes(["extension", "theme"]);
    addons = addons.filter(
      a => !a.hidden && (isDisabledLegacy(a) || isDisabledUnsigned(a))
    );

    this._listBox.textContent = "";

    let elements = addons.map(a => createItem(a));
    if (elements.length == 0) {
      gViewController.loadView("addons://list/extension");
      return;
    }

    sortElements(elements, ["uiState", "name"], true);
    for (let element of elements) {
      this._listBox.appendChild(element);
    }

    gViewController.notifyViewChanged();
  },

  hide() {
    doPendingUninstalls(this._listBox);
  },

  getSelectedAddon() {
    var item = this._listBox.selectedItem;
    if (item) {
      return item.mAddon;
    }
    return null;
  },

  async refreshVisibility() {
    if (legacyExtensionsEnabled) {
      this._categoryItem.disabled = true;
      return;
    }

    let extensions = await AddonManager.getAddonsByTypes([
      "extension",
      "theme",
    ]);

    let haveUnsigned = false;
    let haveLegacy = false;
    for (let extension of extensions) {
      if (isDisabledUnsigned(extension)) {
        haveUnsigned = true;
      }
      if (isLegacyExtension(extension)) {
        haveLegacy = true;
      }
    }

    if (haveLegacy || haveUnsigned) {
      this._categoryItem.disabled = false;
      let name = gStrings.ext.GetStringFromName(
        `type.${haveUnsigned ? "unsupported" : "legacy"}.name`
      );
      this._categoryItem.setAttribute("name", name);
      this._categoryItem.tooltiptext = name;
    } else {
      this._categoryItem.disabled = true;
    }
  },

  getListItemForID(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (
        listitem.getAttribute("status") == "installed" &&
        listitem.mAddon.id == aId
      ) {
        return listitem;
      }
      listitem = listitem.nextSibling;
    }
    return null;
  },
};

var gListView = {
  node: null,
  _listBox: null,
  _emptyNotice: null,
  _type: null,
  isRoot: true,

  initialize() {
    this.node = document.getElementById("list-view");
    this._listBox = document.getElementById("addon-list");
    this._emptyNotice = document.getElementById("addon-list-empty");

    this._listBox.addEventListener("keydown", aEvent => {
      if (aEvent.keyCode == aEvent.DOM_VK_RETURN) {
        var item = this._listBox.selectedItem;
        if (item) {
          item.showInDetailView();
        }
      }
    });
    this._listBox.addEventListener("Uninstall", event =>
      recordActionTelemetry({
        action: "uninstall",
        view: "list",
        addon: event.target.mAddon,
      })
    );
    this._listBox.addEventListener("Undo", event =>
      recordActionTelemetry({ action: "undo", addon: event.target.mAddon })
    );

    document
      .getElementById("signing-learn-more")
      .setAttribute("href", SUPPORT_URL + "unsigned-addons");
    document
      .getElementById("legacy-extensions-learnmore-link")
      .addEventListener("click", evt => {
        gViewController.loadView("addons://legacy/");
      });

    try {
      document
        .getElementById("private-browsing-learnmore-link")
        .setAttribute("href", SUPPORT_URL + "extensions-pb");
    } catch (e) {
      document.getElementById("private-browsing-notice").hidden = true;
    }

    let findSignedAddonsLink = document.getElementById(
      "find-alternative-addons"
    );
    try {
      findSignedAddonsLink.setAttribute(
        "href",
        Services.urlFormatter.formatURLPref("extensions.getAddons.link.url")
      );
    } catch (e) {
      findSignedAddonsLink.classList.remove("text-link");
    }

    try {
      document
        .getElementById("signing-dev-manual-link")
        .setAttribute(
          "href",
          Services.prefs.getCharPref("xpinstall.signatures.devInfoURL")
        );
    } catch (e) {
      document.getElementById("signing-dev-info").hidden = true;
    }

    if (Preferences.get("plugin.load_flash_only", true)) {
      document
        .getElementById("plugindeprecation-learnmore-link")
        .setAttribute("href", SUPPORT_URL + "npapi");
    } else {
      document.getElementById("plugindeprecation-notice").hidden = true;
    }
  },

  show(aType, aRequest) {
    let showOnlyDisabledUnsigned = false;
    if (aType.endsWith("?unsigned=true")) {
      aType = aType.replace(/\?.*/, "");
      showOnlyDisabledUnsigned = true;
    }

    if (!(aType in AddonManager.addonTypes)) {
      throw Components.Exception(
        "Attempting to show unknown type " + aType,
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this._type = aType;
    this.node.setAttribute("type", aType);
    this.showEmptyNotice(false);

    this._listBox.textContent = "";

    if (aType == "plugin") {
      navigator.plugins.refresh(false);
    }

    getAddonsAndInstalls(aType, (aAddonsList, aInstallsList) => {
      if (gViewController && aRequest != gViewController.currentViewRequest) {
        return;
      }

      let showLegacyInfo = false;
      if (!legacyExtensionsEnabled && aType != "locale") {
        let preLen = aAddonsList.length;
        aAddonsList = aAddonsList.filter(
          addon => !isLegacyExtension(addon) && !isDisabledUnsigned(addon)
        );
        if (aAddonsList.length != preLen) {
          showLegacyInfo = true;
        }
      }

      let privateNotice = document.getElementById("private-browsing-notice");
      privateNotice.hidden =
        allowPrivateBrowsingByDefault || aType != "extension";

      var elements = [];

      for (let addonItem of aAddonsList) {
        elements.push(createItem(addonItem));
      }

      for (let installItem of aInstallsList) {
        elements.push(createItem(installItem, true));
      }

      this.showEmptyNotice(elements.length == 0);
      if (elements.length > 0) {
        let sortBy;
        if (aType == "theme") {
          sortBy = ["uiState", "hasPreview", "name"];
        } else {
          sortBy = ["uiState", "name"];
        }
        sortElements(elements, sortBy, true);
        for (let element of elements) {
          this._listBox.appendChild(element);
        }
      }

      this.filterDisabledUnsigned(showOnlyDisabledUnsigned);
      let legacyNotice = document.getElementById("legacy-extensions-notice");
      if (showLegacyInfo) {
        let el = document.getElementById("legacy-extensions-description");
        if (el.childNodes[0].nodeName == "#text") {
          el.removeChild(el.childNodes[0]);
        }

        let descriptionId =
          aType == "theme"
            ? "legacyThemeWarning.description"
            : "legacyWarning.description";
        let text =
          gStrings.ext.formatStringFromName(descriptionId, [
            gStrings.brandShortName,
          ]) + " ";
        el.insertBefore(document.createTextNode(text), el.childNodes[0]);
        legacyNotice.hidden = false;
      } else {
        legacyNotice.hidden = true;
      }

      gEventManager.registerInstallListener(this);
      gViewController.updateCommands();
      gViewController.notifyViewChanged();
    });
  },

  hide() {
    gEventManager.unregisterInstallListener(this);
    doPendingUninstalls(this._listBox);
  },

  filterDisabledUnsigned(aFilter = true) {
    let foundDisabledUnsigned = false;

    for (let item of this._listBox.childNodes) {
      if (isDisabledUnsigned(item.mAddon)) {
        foundDisabledUnsigned = true;
      } else {
        item.hidden = aFilter;
      }
    }

    document.getElementById("show-disabled-unsigned-extensions").hidden =
      aFilter || !foundDisabledUnsigned;

    document.getElementById("show-all-extensions").hidden = !aFilter;
    document.getElementById("disabled-unsigned-addons-info").hidden = !aFilter;
  },

  showEmptyNotice(aShow) {
    this._emptyNotice.hidden = !aShow;
    this._listBox.hidden = aShow;
  },

  onSortChanged(aSortBy, aAscending) {
    sortList(this._listBox, aSortBy, aAscending);
  },

  onExternalInstall(aAddon, aExistingAddon) {
    // The existing list item will take care of upgrade installs
    if (aExistingAddon) {
      return;
    }

    if (aAddon.hidden) {
      return;
    }

    this.addItem(aAddon);
  },

  onDownloadStarted(aInstall) {
    this.addItem(aInstall, true);
  },

  onInstallStarted(aInstall) {
    this.addItem(aInstall, true);
  },

  onDownloadCancelled(aInstall) {
    this.removeItem(aInstall, true);
  },

  onInstallCancelled(aInstall) {
    this.removeItem(aInstall, true);
  },

  onInstallEnded(aInstall) {
    // Remove any install entries for upgrades, their status will appear against
    // the existing item
    if (aInstall.existingAddon) {
      this.removeItem(aInstall, true);
    }
  },

  addItem(aObj, aIsInstall) {
    if (aObj.type != this._type) {
      return;
    }

    if (aIsInstall && aObj.existingAddon) {
      return;
    }

    if (aObj.addon && aObj.addon.hidden) {
      return;
    }

    let prop = aIsInstall ? "mInstall" : "mAddon";
    for (let item of this._listBox.childNodes) {
      if (item[prop] == aObj) {
        return;
      }
    }

    let item = createItem(aObj, aIsInstall);
    this._listBox.insertBefore(item, this._listBox.firstChild);
    this.showEmptyNotice(false);
  },

  removeItem(aObj, aIsInstall) {
    let prop = aIsInstall ? "mInstall" : "mAddon";

    for (let item of this._listBox.childNodes) {
      if (item[prop] == aObj) {
        this._listBox.removeChild(item);
        this.showEmptyNotice(this._listBox.itemCount == 0);
        return;
      }
    }
  },

  getSelectedAddon() {
    var item = this._listBox.selectedItem;
    if (item) {
      return item.mAddon;
    }
    return null;
  },

  getListItemForID(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (
        listitem.getAttribute("status") == "installed" &&
        listitem.mAddon.id == aId
      ) {
        return listitem;
      }
      listitem = listitem.nextSibling;
    }
    return null;
  },
};

var gDetailView = {
  node: null,
  _addon: null,
  _loadingTimer: null,
  _autoUpdate: null,
  isRoot: false,
  restartingAddon: false,

  initialize() {
    this.node = document.getElementById("detail-view");
    this.node.addEventListener("click", this.recordClickTelemetry);
    this.headingImage = this.node.querySelector(".card-heading-image");

    this._autoUpdate = document.getElementById("detail-autoUpdate");
    this._autoUpdate.addEventListener(
      "command",
      () => {
        this._addon.applyBackgroundUpdates = this._autoUpdate.value;
        recordSetAddonUpdateTelemetry(this._addon);
      },
      true
    );

    for (let el of document.getElementsByClassName("private-learnmore")) {
      el.setAttribute("href", SUPPORT_URL + "extensions-pb");
    }

    this._privateBrowsing = document.getElementById("detail-privateBrowsing");
    this._privateBrowsing.addEventListener(
      "command",
      async () => {
        let addon = this._addon;
        let policy = WebExtensionPolicy.getByID(addon.id);
        let extension = policy && policy.extension;

        let perms = {
          permissions: ["internal:privateBrowsingAllowed"],
          origins: [],
        };
        if (this._privateBrowsing.value == "1") {
          await ExtensionPermissions.add(addon.id, perms, extension);
          recordActionTelemetry({
            action: "privateBrowsingAllowed",
            value: "on",
            addon,
          });
        } else {
          await ExtensionPermissions.remove(addon.id, perms, extension);
          recordActionTelemetry({
            action: "privateBrowsingAllowed",
            value: "off",
            addon,
          });
        }

        // Reload the extension if it is already enabled.  This ensures any change
        // on the private browsing permission is properly handled.
        if (addon.isActive) {
          try {
            this.restartingAddon = true;
            await addon.reload();
          } finally {
            this.restartingAddon = false;
            this.updateState();
            this._updateView(addon, false);
          }
        }
      },
      true
    );
  },

  shutdown() {
    AddonManager.removeManagerListener(this);
  },

  onUpdateModeChanged() {
    this.onPropertyChanged(["applyBackgroundUpdates"]);
  },

  recordClickTelemetry(event) {
    if (event.target.id == "detail-reviews") {
      recordLinkTelemetry("rating");
    } else if (event.target.id == "detail-homepage") {
      recordLinkTelemetry("homepage");
    } else if (event.originalTarget.getAttribute("anonid") == "creator-link") {
      recordLinkTelemetry("author");
    }
  },

  async _updateView(aAddon, aIsRemote, aScrollToPreferences) {
    // Skip updates to avoid flickering while restarting the addon.
    if (this.restartingAddon) {
      return;
    }

    setSearchLabel(aAddon.type);

    // Set the preview image for themes, if available.
    this.headingImage.src = "";
    if (aAddon.type == "theme") {
      let previewURL =
        aAddon.screenshots &&
        aAddon.screenshots[0] &&
        aAddon.screenshots[0].url;
      if (previewURL) {
        this.headingImage.src = previewURL;
      }
    }

    AddonManager.addManagerListener(this);
    this.clearLoading();

    this._addon = aAddon;
    gEventManager.registerAddonListener(this, aAddon.id);
    gEventManager.registerInstallListener(this);

    this.node.setAttribute("type", aAddon.type);

    let legacy = false;
    if (!aAddon.install) {
      legacy = isLegacyExtension(aAddon);
    }
    this.node.setAttribute("legacy", legacy);
    document.getElementById("detail-legacy-warning").href =
      SUPPORT_URL + "webextensions";

    // Make sure to select the correct category
    let category =
      isDisabledLegacy(aAddon) || isDisabledUnsigned(aAddon)
        ? "addons://legacy"
        : `addons://list/${aAddon.type}`;
    gCategories.select(category);

    document.getElementById("detail-name").textContent = aAddon.name;
    var icon = AddonManager.getPreferredIconURL(aAddon, 32, window);
    document.getElementById("detail-icon").src = icon ? icon : "";
    document
      .getElementById("detail-creator")
      .setCreator(aAddon.creator, aAddon.homepageURL);

    var version = document.getElementById("detail-version");
    if (shouldShowVersionNumber(aAddon)) {
      version.hidden = false;
      version.value = aAddon.version;
    } else {
      version.hidden = true;
    }

    var desc = document.getElementById("detail-desc");
    desc.textContent = aAddon.description;

    var fullDesc = document.getElementById("detail-fulldesc");
    if (aAddon.getFullDescription) {
      fullDesc.textContent = "";
      fullDesc.append(aAddon.getFullDescription(document));
      fullDesc.hidden = false;
    } else if (aAddon.fullDescription) {
      fullDesc.textContent = aAddon.fullDescription;
      fullDesc.hidden = false;
    } else {
      fullDesc.hidden = true;
    }

    var contributions = document.getElementById("detail-contributions");
    if ("contributionURL" in aAddon && aAddon.contributionURL) {
      contributions.hidden = false;
    } else {
      contributions.hidden = true;
    }

    var updateDateRow = document.getElementById("detail-dateUpdated");
    if (aAddon.updateDate) {
      var date = formatDate(aAddon.updateDate);
      updateDateRow.value = date;
    } else {
      updateDateRow.value = null;
    }

    // TODO if the add-on was downloaded from releases.mozilla.org link to the
    // AMO profile (bug 590344)
    if (false) {
      document.getElementById("detail-repository-row").hidden = false;
      document.getElementById("detail-homepage-row").hidden = true;
      var repository = document.getElementById("detail-repository");
      repository.value = aAddon.homepageURL;
      repository.href = aAddon.homepageURL;
    } else if (aAddon.homepageURL) {
      document.getElementById("detail-repository-row").hidden = true;
      document.getElementById("detail-homepage-row").hidden = false;
      var homepage = document.getElementById("detail-homepage");
      homepage.value = aAddon.homepageURL;
      homepage.href = aAddon.homepageURL;
    } else {
      document.getElementById("detail-repository-row").hidden = true;
      document.getElementById("detail-homepage-row").hidden = true;
    }

    var rating = document.getElementById("detail-rating");
    if (aAddon.averageRating) {
      rating.averageRating = aAddon.averageRating;
      rating.hidden = false;
    } else {
      rating.hidden = true;
    }

    var reviews = document.getElementById("detail-reviews");
    if (aAddon.reviewURL) {
      var text = gStrings.ext.GetStringFromName("numReviews");
      text = PluralForm.get(aAddon.reviewCount, text);
      text = text.replace("#1", aAddon.reviewCount);
      reviews.value = text;
      reviews.hidden = false;
      reviews.href = aAddon.reviewURL;
    } else {
      reviews.hidden = true;
    }

    document.getElementById("detail-rating-row").hidden =
      !aAddon.averageRating && !aAddon.reviewURL;

    var canUpdate = !aIsRemote && hasPermission(aAddon, "upgrade");
    document.getElementById("detail-updates-row").hidden = !canUpdate;

    if ("applyBackgroundUpdates" in aAddon) {
      this._autoUpdate.hidden = false;
      this._autoUpdate.value = aAddon.applyBackgroundUpdates;
      let hideFindUpdates = AddonManager.shouldAutoUpdate(this._addon);
      document.getElementById(
        "detail-findUpdates-btn"
      ).hidden = hideFindUpdates;
    } else {
      this._autoUpdate.hidden = true;
      document.getElementById("detail-findUpdates-btn").hidden = false;
    }

    // Only type = "extension" will ever get privateBrowsingAllowed, other types have
    // no code that would be affected by the setting.  The permission is read directly
    // from ExtensionPermissions so we can get it whether or not the extension is
    // currently active.
    // Ensure that all private browsing rows are hidden by default, we'll then
    // unhide what we want.
    for (let el of document.getElementsByClassName("detail-privateBrowsing")) {
      el.hidden = true;
    }
    if (!allowPrivateBrowsingByDefault && aAddon.type === "extension") {
      if (
        aAddon.permissions & AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS
      ) {
        let privateBrowsingRow = document.getElementById(
          "detail-privateBrowsing-row"
        );
        let privateBrowsingFooterRow = document.getElementById(
          "detail-privateBrowsing-row-footer"
        );
        let perms = await ExtensionPermissions.get(aAddon.id);
        this._privateBrowsing.hidden = false;
        privateBrowsingRow.hidden = false;
        privateBrowsingFooterRow.hidden = false;
        this._privateBrowsing.value = perms.permissions.includes(
          "internal:privateBrowsingAllowed"
        )
          ? "1"
          : "0";
      } else if (aAddon.incognito == "spanning") {
        document.getElementById(
          "detail-privateBrowsing-required"
        ).hidden = false;
        document.getElementById(
          "detail-privateBrowsing-required-footer"
        ).hidden = false;
      } else if (aAddon.incognito == "not_allowed") {
        document.getElementById(
          "detail-privateBrowsing-disallowed"
        ).hidden = false;
        document.getElementById(
          "detail-privateBrowsing-disallowed-footer"
        ).hidden = false;
      }
    }

    // While updating the addon details view, also check if the preferences button should be disabled because
    // we are in a private window and the addon is not allowed to access it.
    let hidePreferences =
      (!aIsRemote &&
        !gViewController.commands.cmd_showItemPreferences.isEnabled(aAddon)) ||
      !(await isAddonAllowedInCurrentWindow(aAddon));
    document.getElementById("detail-prefs-btn").hidden = hidePreferences;

    var gridRows = document.querySelectorAll("#detail-grid rows row");
    let first = true;
    for (let gridRow of gridRows) {
      if (
        first &&
        window.getComputedStyle(gridRow).getPropertyValue("display") != "none"
      ) {
        gridRow.setAttribute("first-row", true);
        first = false;
      } else {
        gridRow.removeAttribute("first-row");
      }
    }

    this.fillSettingsRows(aScrollToPreferences, () => {
      this.updateState();
      gViewController.notifyViewChanged();
    });
  },

  async show(aAddonId, aRequest) {
    let index = aAddonId.indexOf("/preferences");
    let scrollToPreferences = false;
    if (index >= 0) {
      aAddonId = aAddonId.substring(0, index);
      scrollToPreferences = true;
    }

    this._loadingTimer = setTimeout(() => {
      this.node.setAttribute("loading-extended", true);
    }, LOADING_MSG_DELAY);

    let aAddon = await AddonManager.getAddonByID(aAddonId);
    if (gViewController && aRequest != gViewController.currentViewRequest) {
      return;
    }

    if (aAddon) {
      this._updateView(aAddon, false, scrollToPreferences);
      return;
    }

    // Look for an add-on pending install
    let aInstalls = await AddonManager.getAllInstalls();
    for (let install of aInstalls) {
      if (
        install.state == AddonManager.STATE_INSTALLED &&
        install.addon.id == aAddonId
      ) {
        this._updateView(install.addon, false);
        return;
      }
    }

    // This might happen due to session restore restoring us back to an
    // add-on that doesn't exist but otherwise shouldn't normally happen.
    // Either way just revert to the default view.
    gViewController.replaceView(gViewDefault);
  },

  hide() {
    AddonManager.removeManagerListener(this);
    this.clearLoading();
    if (this._addon) {
      if (hasInlineOptions(this._addon)) {
        Services.obs.notifyObservers(
          document,
          AddonManager.OPTIONS_NOTIFICATION_HIDDEN,
          this._addon.id
        );
      }

      gEventManager.unregisterAddonListener(this, this._addon.id);
      gEventManager.unregisterInstallListener(this);
      this._addon = null;

      // Flush the preferences to disk so they survive any crash
      if (this.node.getElementsByTagName("setting").length) {
        Services.prefs.savePrefFile(null);
      }
    }
  },

  updateState() {
    // Skip updates to avoid flickering while restarting the addon.
    if (this.restartingAddon) {
      return;
    }

    gViewController.updateCommands();

    var pending = this._addon.pendingOperations;
    if (pending & AddonManager.PENDING_UNINSTALL) {
      this.node.removeAttribute("notification");

      // We don't care about pending operations other than uninstall.
      // They're transient, and cannot be undone.
      this.node.setAttribute("pending", "uninstall");
      document.getElementById(
        "detail-pending"
      ).textContent = gStrings.ext.formatStringFromName(
        "details.notification.restartless-uninstall",
        [this._addon.name]
      );
    } else {
      this.node.removeAttribute("pending");

      if (this._addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED) {
        this.node.setAttribute("notification", "error");
        document.getElementById(
          "detail-error"
        ).textContent = gStrings.ext.formatStringFromName(
          "details.notification.blocked",
          [this._addon.name]
        );
        var errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName(
          "details.notification.blocked.link"
        );
        this._addon.getBlocklistURL().then(url => {
          errorLink.href = url;
          errorLink.hidden = false;
        });
      } else if (isDisabledUnsigned(this._addon)) {
        this.node.setAttribute("notification", "error");
        document.getElementById(
          "detail-error"
        ).textContent = gStrings.ext.formatStringFromName(
          "details.notification.unsignedAndDisabled",
          [this._addon.name, gStrings.brandShortName]
        );
        let errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName(
          "details.notification.unsigned.link"
        );
        errorLink.href = SUPPORT_URL + "unsigned-addons";
        errorLink.hidden = false;
      } else if (
        !this._addon.isCompatible &&
        (AddonManager.checkCompatibility ||
          this._addon.blocklistState !=
            Ci.nsIBlocklistService.STATE_SOFTBLOCKED)
      ) {
        this.node.setAttribute("notification", "warning");
        document.getElementById(
          "detail-warning"
        ).textContent = gStrings.ext.formatStringFromName(
          "details.notification.incompatible",
          [this._addon.name, gStrings.brandShortName, gStrings.appVersion]
        );
        document.getElementById("detail-warning-link").hidden = true;
      } else if (!isCorrectlySigned(this._addon)) {
        this.node.setAttribute("notification", "warning");
        document.getElementById(
          "detail-warning"
        ).textContent = gStrings.ext.formatStringFromName(
          "details.notification.unsigned",
          [this._addon.name, gStrings.brandShortName]
        );
        var warningLink = document.getElementById("detail-warning-link");
        warningLink.value = gStrings.ext.GetStringFromName(
          "details.notification.unsigned.link"
        );
        warningLink.href = SUPPORT_URL + "unsigned-addons";
        warningLink.hidden = false;
      } else if (
        this._addon.blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED
      ) {
        this.node.setAttribute("notification", "warning");
        document.getElementById(
          "detail-warning"
        ).textContent = gStrings.ext.formatStringFromName(
          "details.notification.softblocked",
          [this._addon.name]
        );
        let warningLink = document.getElementById("detail-warning-link");
        warningLink.value = gStrings.ext.GetStringFromName(
          "details.notification.softblocked.link"
        );
        this._addon.getBlocklistURL().then(url => {
          warningLink.href = url;
          warningLink.hidden = false;
        });
      } else if (
        this._addon.blocklistState == Ci.nsIBlocklistService.STATE_OUTDATED
      ) {
        this.node.setAttribute("notification", "warning");
        document.getElementById(
          "detail-warning"
        ).textContent = gStrings.ext.formatStringFromName(
          "details.notification.outdated",
          [this._addon.name]
        );
        let warningLink = document.getElementById("detail-warning-link");
        warningLink.value = gStrings.ext.GetStringFromName(
          "details.notification.outdated.link"
        );
        this._addon.getBlocklistURL().then(url => {
          warningLink.href = url;
          warningLink.hidden = false;
        });
      } else if (
        this._addon.blocklistState ==
        Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE
      ) {
        this.node.setAttribute("notification", "error");
        document.getElementById(
          "detail-error"
        ).textContent = gStrings.ext.formatStringFromName(
          "details.notification.vulnerableUpdatable",
          [this._addon.name]
        );
        let errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName(
          "details.notification.vulnerableUpdatable.link"
        );
        this._addon.getBlocklistURL().then(url => {
          errorLink.href = url;
          errorLink.hidden = false;
        });
      } else if (
        this._addon.blocklistState ==
        Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE
      ) {
        this.node.setAttribute("notification", "error");
        document.getElementById(
          "detail-error"
        ).textContent = gStrings.ext.formatStringFromName(
          "details.notification.vulnerableNoUpdate",
          [this._addon.name]
        );
        let errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName(
          "details.notification.vulnerableNoUpdate.link"
        );
        this._addon.getBlocklistURL().then(url => {
          errorLink.href = url;
          errorLink.hidden = false;
        });
      } else if (
        this._addon.isGMPlugin &&
        !this._addon.isInstalled &&
        this._addon.isActive
      ) {
        this.node.setAttribute("notification", "warning");
        let warning = document.getElementById("detail-warning");
        warning.textContent = gStrings.ext.formatStringFromName(
          "details.notification.gmpPending",
          [this._addon.name]
        );
      } else {
        this.node.removeAttribute("notification");
      }
    }

    let menulist = document.getElementById("detail-state-menulist");
    let addonType = AddonManager.addonTypes[this._addon.type];
    if (addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) {
      let askItem = document.getElementById("detail-ask-to-activate-menuitem");
      let alwaysItem = document.getElementById(
        "detail-always-activate-menuitem"
      );
      let neverItem = document.getElementById("detail-never-activate-menuitem");
      let hasActivatePermission = ["ask_to_activate", "enable", "disable"].some(
        perm => hasPermission(this._addon, perm)
      );

      if (!this._addon.isActive) {
        menulist.selectedItem = neverItem;
      } else if (
        this._addon.userDisabled == AddonManager.STATE_ASK_TO_ACTIVATE
      ) {
        menulist.selectedItem = askItem;
      } else {
        menulist.selectedItem = alwaysItem;
      }

      menulist.disabled = !hasActivatePermission;
      menulist.hidden = false;
      menulist.classList.add("no-auto-hide");
    } else {
      menulist.hidden = true;
    }

    this.node.setAttribute("active", this._addon.isActive);
  },

  clearLoading() {
    if (this._loadingTimer) {
      clearTimeout(this._loadingTimer);
      this._loadingTimer = null;
    }

    this.node.removeAttribute("loading-extended");
  },

  emptySettingsRows() {
    var lastRow = document.getElementById("detail-rating-row");
    var rows = lastRow.parentNode;
    while (lastRow.nextSibling) {
      rows.removeChild(rows.lastChild);
    }
  },

  fillSettingsRows(aScrollToPreferences, aCallback) {
    this.emptySettingsRows();
    if (!hasInlineOptions(this._addon)) {
      if (aCallback) {
        aCallback();
      }
      return;
    }

    // We can't use a promise for this, since some code (especially in tests)
    // relies on us finishing before the ViewChanged event bubbles up to its
    // listeners, and promises resolve asynchronously.
    let whenViewLoaded = callback => {
      if (gViewController.displayedView.hasAttribute("loading")) {
        gDetailView.node.addEventListener(
          "ViewChanged",
          function() {
            callback();
          },
          { once: true }
        );
      } else {
        callback();
      }
    };

    let finish = firstSetting => {
      // Ensure the page has loaded and force the XBL bindings to be synchronously applied,
      // then notify observers.
      whenViewLoaded(() => {
        if (firstSetting) {
          firstSetting.clientTop;
        }
        Services.obs.notifyObservers(
          document,
          AddonManager.OPTIONS_NOTIFICATION_DISPLAYED,
          this._addon.id
        );
        if (aScrollToPreferences) {
          gDetailView.scrollToPreferencesRows();
        }
      });
    };

    var rows = document.getElementById("detail-rows");

    if (this._addon.optionsType == AddonManager.OPTIONS_TYPE_INLINE_BROWSER) {
      whenViewLoaded(async () => {
        const addon = this._addon;
        await addon.startupPromise;

        // Do not create the inline addon options if about:addons is opened in a private window
        // and the addon is not allowed to access it.
        if (!(await isAddonAllowedInCurrentWindow(addon))) {
          return;
        }

        const browserContainer = await this.createOptionsBrowser(rows);

        if (browserContainer) {
          // Make sure the browser is unloaded as soon as we change views,
          // rather than waiting for the next detail view to load.
          document.addEventListener(
            "ViewChanged",
            function() {
              // Do not remove the addon options container if the view changed
              // event is not related to a change to the current selected view
              // or the current selected addon (e.g. it could be related to the
              // disco pane view that has completed to load, See Bug 1435705 for
              // a rationale).
              if (
                gViewController.currentViewObj === gDetailView &&
                gDetailView._addon === addon
              ) {
                return;
              }
              browserContainer.remove();
            },
            { once: true }
          );
        }

        finish(browserContainer);
      });
    }

    if (aCallback) {
      aCallback();
    }
  },

  scrollToPreferencesRows() {
    // We find this row, rather than remembering it from above,
    // in case it has been changed by the observers.
    let firstRow = gDetailView.node.querySelector('setting[first-row="true"]');
    if (firstRow) {
      let top = firstRow.getBoundingClientRect().y;
      top -= parseInt(
        window.getComputedStyle(firstRow).getPropertyValue("margin-top")
      );

      let detailView = gDetailView.node;
      top -= detailView.getBoundingClientRect().y;

      detailView.scrollTo(0, top);
    }
  },

  async createOptionsBrowser(parentNode) {
    const containerId = "addon-options-prompts-stack";

    let stack = document.getElementById(containerId);

    if (stack) {
      // Remove the existent options container (if any).
      stack.remove();
    }

    stack = document.createXULElement("stack");
    stack.setAttribute("id", containerId);

    let browser = document.createXULElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("id", "addon-options");
    browser.setAttribute("class", "inline-options-browser");
    browser.setAttribute("transparent", "true");
    browser.setAttribute("forcemessagemanager", "true");
    browser.setAttribute("selectmenulist", "ContentSelectDropdown");
    browser.setAttribute("autocompletepopup", "PopupAutoComplete");

    // The outer about:addons document listens for key presses to focus
    // the search box when / is pressed.  But if we're focused inside an
    // options page, don't let those keypresses steal focus.
    browser.addEventListener("keypress", event => {
      event.stopPropagation();
    });

    let { optionsURL, optionsBrowserStyle } = this._addon;
    if (this._addon.isWebExtension) {
      let policy = ExtensionParent.WebExtensionPolicy.getByID(this._addon.id);
      browser.sameProcessAsFrameLoader = policy.extension.groupFrameLoader;
    }

    let remoteSubframes = window.docShell.QueryInterface(Ci.nsILoadContext)
      .useRemoteSubframes;

    let readyPromise;
    if (
      E10SUtils.canLoadURIInRemoteType(
        optionsURL,
        remoteSubframes,
        E10SUtils.EXTENSION_REMOTE_TYPE
      )
    ) {
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", E10SUtils.EXTENSION_REMOTE_TYPE);
      readyPromise = promiseEvent("XULFrameLoaderCreated", browser);

      readyPromise.then(() => {
        if (!browser.messageManager) {
          // Early exit if the the extension page's XUL browser has been destroyed in the meantime
          // (e.g. because the extension has been reloaded while the options page was still loading).
          return;
        }
        const parentChromeWindow = window.docShell.parent.domWindow;
        const parentContextMenuPopup = parentChromeWindow.document.getElementById(
          "contentAreaContextMenu"
        );

        // Override openPopupAtScreen on the dummy menupopup element, so that we can forward
        // "nsContextMenu.js openContextMenu"'s calls related to the extensions "options page"
        // context menu events.
        document.getElementById("contentAreaContextMenu").openPopupAtScreen = (
          ...args
        ) => {
          return parentContextMenuPopup.openPopupAtScreen(...args);
        };
      });
    } else {
      readyPromise = promiseEvent("load", browser, true);
    }

    stack.appendChild(browser);
    parentNode.appendChild(stack);

    // Force bindings to apply synchronously.
    browser.clientTop;

    await readyPromise;

    if (!browser.messageManager) {
      // If the browser.messageManager is undefined, the browser element has been
      // removed from the document in the meantime (e.g. due to a rapid sequence
      // of addon reload), ensure that the stack is also removed and return null.
      stack.remove();
      return null;
    }

    ExtensionParent.apiManager.emit("extension-browser-inserted", browser);

    return new Promise(resolve => {
      let messageListener = {
        receiveMessage({ name, data }) {
          if (name === "Extension:BrowserResized") {
            browser.style.height = `${data.height}px`;
          } else if (name === "Extension:BrowserContentLoaded") {
            resolve(stack);
          }
        },
      };

      let mm = browser.messageManager;

      if (!mm) {
        // If the browser.messageManager is undefined, the browser element has been
        // removed from the document in the meantime (e.g. due to a rapid sequence
        // of addon reload), ensure that the stack is also removed and return null.
        stack.remove();
        resolve(null);
        return;
      }

      mm.loadFrameScript(
        "chrome://extensions/content/ext-browser-content.js",
        false,
        true
      );
      mm.loadFrameScript("chrome://browser/content/content.js", false, true);
      mm.addMessageListener("Extension:BrowserContentLoaded", messageListener);
      mm.addMessageListener("Extension:BrowserResized", messageListener);

      let browserOptions = {
        fixedWidth: true,
        isInline: true,
      };

      if (optionsBrowserStyle) {
        browserOptions.stylesheets = extensionStylesheets;
      }

      mm.sendAsyncMessage("Extension:InitBrowser", browserOptions);

      browser.loadURI(optionsURL, {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    });
  },

  getSelectedAddon() {
    return this._addon;
  },

  onEnabling() {
    this.updateState();
  },

  onEnabled() {
    this.updateState();
    this.fillSettingsRows();
  },

  onDisabling() {
    this.updateState();
    if (hasInlineOptions(this._addon)) {
      Services.obs.notifyObservers(
        document,
        AddonManager.OPTIONS_NOTIFICATION_HIDDEN,
        this._addon.id
      );
    }
  },

  onDisabled() {
    this.updateState();
    this.emptySettingsRows();
  },

  onUninstalling() {
    this.updateState();
  },

  onUninstalled() {
    gViewController.popState();
  },

  onOperationCancelled() {
    this.updateState();
  },

  onPropertyChanged(aProperties) {
    if (aProperties.includes("applyBackgroundUpdates")) {
      this._autoUpdate.value = this._addon.applyBackgroundUpdates;
      let hideFindUpdates = AddonManager.shouldAutoUpdate(this._addon);
      document.getElementById(
        "detail-findUpdates-btn"
      ).hidden = hideFindUpdates;
    }

    if (
      aProperties.includes("appDisabled") ||
      aProperties.includes("signedState") ||
      aProperties.includes("userDisabled")
    ) {
      this.updateState();
    }
  },

  onExternalInstall(aAddon, aExistingAddon) {
    // Only care about upgrades for the currently displayed add-on
    if (!aExistingAddon || aExistingAddon.id != this._addon.id) {
      return;
    }

    this._updateView(aAddon, false);
  },

  onInstallCancelled(aInstall) {
    if (aInstall.addon.id == this._addon.id) {
      gViewController.popState();
    }
  },
};

var gUpdatesView = {
  node: null,
  _listBox: null,
  _emptyNotice: null,
  _updateSelected: null,
  _categoryItem: null,
  isRoot: true,

  initialize() {
    this.node = document.getElementById("updates-view");
    this._listBox = document.getElementById("updates-list");
    this._emptyNotice = document.getElementById("updates-list-empty");

    this._categoryItem = gCategories.get("addons://updates/available");

    this._updateSelected = document.getElementById("update-selected-btn");
    this._updateSelected.addEventListener("command", function() {
      gUpdatesView.installSelected();
    });
    this.node.addEventListener("RelNotesShow", event => {
      recordActionTelemetry({
        action: "releaseNotes",
        addon: event.target.mAddon,
      });
    });

    this.updateAvailableCount(true);

    AddonManager.addAddonListener(this);
    AddonManager.addInstallListener(this);
  },

  shutdown() {
    AddonManager.removeAddonListener(this);
    AddonManager.removeInstallListener(this);
  },

  show(aType, aRequest) {
    document.getElementById("empty-availableUpdates-msg").hidden =
      aType != "available";
    document.getElementById("empty-recentUpdates-msg").hidden =
      aType != "recent";
    this.showEmptyNotice(false);

    this._listBox.textContent = "";

    this.node.setAttribute("updatetype", aType);
    if (aType == "recent") {
      this._showRecentUpdates(aRequest);
    } else {
      this._showAvailableUpdates(false, aRequest);
    }
  },

  hide() {
    this._updateSelected.hidden = true;
    this._categoryItem.hidden = this._categoryItem.badgeCount == 0;
    doPendingUninstalls(this._listBox);
  },

  async _showRecentUpdates(aRequest) {
    let aAddonsList = await AddonManager.getAllAddons();
    if (gViewController && aRequest != gViewController.currentViewRequest) {
      return;
    }

    var elements = [];
    let threshold = Date.now() - UPDATES_RECENT_TIMESPAN;
    for (let addon of aAddonsList) {
      if (
        addon.hidden ||
        !addon.updateDate ||
        addon.updateDate.getTime() < threshold
      ) {
        continue;
      }

      elements.push(createItem(addon));
    }

    this.showEmptyNotice(elements.length == 0);
    if (elements.length > 0) {
      sortElements(elements, ["updateDate"], false);
      for (let element of elements) {
        this._listBox.appendChild(element);
      }
    }

    gViewController.notifyViewChanged();
  },

  async _showAvailableUpdates(aIsRefresh, aRequest) {
    /* Disable the Update Selected button so it can't get clicked
       before everything is initialized asynchronously.
       It will get re-enabled by maybeDisableUpdateSelected(). */
    this._updateSelected.disabled = true;

    let aInstallsList = await AddonManager.getAllInstalls();
    if (
      !aIsRefresh &&
      gViewController &&
      aRequest &&
      aRequest != gViewController.currentViewRequest
    ) {
      return;
    }

    if (aIsRefresh) {
      this.showEmptyNotice(false);
      this._updateSelected.hidden = true;

      while (this._listBox.childNodes.length > 0) {
        this._listBox.firstChild.remove();
      }
    }

    var elements = [];

    for (let install of aInstallsList) {
      if (!this.isManualUpdate(install)) {
        continue;
      }

      let item = createItem(install.existingAddon);
      item.setAttribute("upgrade", true);
      item.addEventListener("IncludeUpdateChanged", () => {
        this.maybeDisableUpdateSelected();
      });
      elements.push(item);
    }

    this.showEmptyNotice(elements.length == 0);
    if (elements.length > 0) {
      this._updateSelected.hidden = false;
      sortElements(elements, ["updateDate"], false);
      for (let element of elements) {
        this._listBox.appendChild(element);
      }
    }

    // ensure badge count is in sync
    this._categoryItem.badgeCount = this._listBox.itemCount;

    gViewController.notifyViewChanged();
  },

  showEmptyNotice(aShow) {
    this._emptyNotice.hidden = !aShow;
    this._listBox.hidden = aShow;
  },

  isManualUpdate(aInstall, aOnlyAvailable) {
    var isManual =
      aInstall.existingAddon &&
      !AddonManager.shouldAutoUpdate(aInstall.existingAddon);
    if (isManual && aOnlyAvailable) {
      return isInState(aInstall, "available");
    }
    return isManual;
  },

  maybeRefresh() {
    if (gViewController.currentViewId == "addons://updates/available") {
      this._showAvailableUpdates(true);
    }
    this.updateAvailableCount();
  },

  async updateAvailableCount(aInitializing) {
    if (aInitializing) {
      gPendingInitializations++;
    }
    let aInstallsList = await AddonManager.getAllInstalls();
    var count = aInstallsList.filter(aInstall => {
      return this.isManualUpdate(aInstall, true);
    }).length;
    this._categoryItem.hidden =
      gViewController.currentViewId != "addons://updates/available" &&
      count == 0;
    this._categoryItem.badgeCount = count;
    if (aInitializing) {
      notifyInitialized();
    }
  },

  maybeDisableUpdateSelected() {
    for (let item of this._listBox.childNodes) {
      if (item.includeUpdate) {
        this._updateSelected.disabled = false;
        return;
      }
    }
    this._updateSelected.disabled = true;
  },

  installSelected() {
    for (let item of this._listBox.childNodes) {
      if (item.includeUpdate) {
        item.upgrade();
      }
    }

    this._updateSelected.disabled = true;
  },

  getSelectedAddon() {
    var item = this._listBox.selectedItem;
    if (item) {
      return item.mAddon;
    }
    return null;
  },

  getListItemForID(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.mAddon.id == aId) {
        return listitem;
      }
      listitem = listitem.nextSibling;
    }
    return null;
  },

  onSortChanged(aSortBy, aAscending) {
    sortList(this._listBox, aSortBy, aAscending);
  },

  onNewInstall(aInstall) {
    if (!this.isManualUpdate(aInstall)) {
      return;
    }
    this.maybeRefresh();
  },

  onInstallStarted(aInstall) {
    this.updateAvailableCount();
  },

  onInstallCancelled(aInstall) {
    if (!this.isManualUpdate(aInstall)) {
      return;
    }
    this.maybeRefresh();
  },

  onPropertyChanged(aAddon, aProperties) {
    if (aProperties.includes("applyBackgroundUpdates")) {
      this.updateAvailableCount();
    }
  },
};

var gShortcutsView = {
  node: null,
  loaded: null,
  isRoot: false,

  initialize() {
    this.node = document.getElementById("shortcuts-view");
    this.node.loadURI("chrome://mozapps/content/extensions/shortcuts.html", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    // Store a Promise for when the contentWindow will exist.
    this.loaded = new Promise(resolve =>
      this.node.addEventListener("load", resolve, { once: true })
    );
  },

  async show() {
    // Ensure the Extensions category is selected in case of refresh/restart.
    gCategories.select("addons://list/extension");

    await this.loaded;
    await this.node.contentWindow.render();
    gViewController.notifyViewChanged();
  },

  hide() {},

  getSelectedAddon() {
    return null;
  },
};

var gDragDrop = {
  onDragOver(aEvent) {
    if (!XPINSTALL_ENABLED) {
      aEvent.dataTransfer.effectAllowed = "none";
      return;
    }
    var types = aEvent.dataTransfer.types;
    if (
      types.includes("text/uri-list") ||
      types.includes("text/x-moz-url") ||
      types.includes("application/x-moz-file")
    ) {
      aEvent.preventDefault();
    }
  },

  async onDrop(aEvent) {
    aEvent.preventDefault();

    let dataTransfer = aEvent.dataTransfer;
    let browser = getBrowserElement();

    // Convert every dropped item into a url and install it
    for (var i = 0; i < dataTransfer.mozItemCount; i++) {
      let url = dataTransfer.mozGetDataAt("text/uri-list", i);
      if (!url) {
        url = dataTransfer.mozGetDataAt("text/x-moz-url", i);
      }
      if (url) {
        url = url.split("\n")[0];
      } else {
        let file = dataTransfer.mozGetDataAt("application/x-moz-file", i);
        if (file) {
          url = Services.io.newFileURI(file).spec;
        }
      }

      if (url) {
        let install = await AddonManager.getInstallForURL(url, {
          telemetryInfo: {
            source: "about:addons",
            method: "drag-and-drop",
          },
        });
        AddonManager.installAddonFromAOM(
          browser,
          document.documentURIObject,
          install
        );
      }
    }
  },
};

// Force the options_ui remote browser to recompute window.mozInnerScreenX and
// window.mozInnerScreenY when the "addon details" page has been scrolled
// (See Bug 1390445 for rationale).
{
  const UPDATE_POSITION_DELAY = 100;

  const updatePositionTask = new DeferredTask(() => {
    const browser = document.getElementById("addon-options");
    if (browser && browser.isRemoteBrowser) {
      browser.frameLoader.requestUpdatePosition();
    }
  }, UPDATE_POSITION_DELAY);

  window.addEventListener(
    "scroll",
    () => {
      updatePositionTask.arm();
    },
    true
  );
}

const addonTypes = new Set([
  "extension",
  "theme",
  "plugin",
  "dictionary",
  "locale",
]);
const htmlViewOpts = {
  loadViewFn(view, sourceEvent) {
    let viewId = `addons://${view}`;
    gViewController.loadView(viewId, sourceEvent);
  },
  replaceWithDefaultViewFn() {
    gViewController.replaceView(gViewDefault);
  },
  setCategoryFn(name) {
    if (addonTypes.has(name)) {
      gCategories.select(`addons://list/${name}`);
    }
  },
};

// View wrappers for the HTML version of about:addons. These delegate to an
// HTML browser that renders the actual views.
let htmlBrowser;
let htmlBrowserLoaded;
function getHtmlBrowser() {
  if (!htmlBrowser) {
    htmlBrowser = document.getElementById("html-view-browser");
    htmlBrowser.loadURI(
      "chrome://mozapps/content/extensions/aboutaddons.html",
      {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      }
    );
    htmlBrowserLoaded = new Promise(resolve =>
      htmlBrowser.addEventListener("load", resolve, { once: true })
    ).then(() => htmlBrowser.contentWindow.initialize(htmlViewOpts));
  }
  return htmlBrowser;
}

function htmlView(type) {
  return {
    _browser: null,
    node: null,
    isRoot: type != "detail",

    initialize() {
      this._browser = getHtmlBrowser();
      this.node = this._browser.closest("#html-view");
    },

    async show(param, request, state) {
      await htmlBrowserLoaded;
      this.node.setAttribute("type", type);
      this.node.setAttribute("param", param);
      await this._browser.contentWindow.show(type, param, state);
      gViewController.updateCommands();
      gViewController.notifyViewChanged();
    },

    async hide() {
      await htmlBrowserLoaded;
      this.node.removeAttribute("type");
      this.node.removeAttribute("param");
      return this._browser.contentWindow.hide();
    },

    getSelectedAddon() {
      return null;
    },
  };
}
