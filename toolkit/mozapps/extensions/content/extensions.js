/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../content/contentAreaUtils.js */
/* globals XMLStylesheetProcessingInstruction */
/* exported UPDATES_RELEASENOTES_TRANSFORMFILE, XMLURI_PARSE_ERROR, loadView */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DownloadUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/addons/AddonRepository.jsm");
Cu.import("resource://gre/modules/addons/AddonSettings.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils", "resource:///modules/E10SUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionParent",
                                  "resource://gre/modules/ExtensionParent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");


XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Experiments",
  "resource:///modules/experiments/Experiments.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "WEBEXT_PERMISSION_PROMPTS",
                                      "extensions.webextPermissionPrompts", false);
XPCOMUtils.defineLazyPreferenceGetter(this, "ALLOW_NON_MPC",
                                      "extensions.allow-non-mpc-extensions", true);

XPCOMUtils.defineLazyPreferenceGetter(this, "SUPPORT_URL", "app.support.baseURL",
                                      "", null, val => Services.urlFormatter.formatURL(val));

const PREF_DISCOVERURL = "extensions.webservice.discoverURL";
const PREF_DISCOVER_ENABLED = "extensions.getAddons.showPane";
const PREF_XPI_ENABLED = "xpinstall.enabled";
const PREF_MAXRESULTS = "extensions.getAddons.maxResults";
const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_GETADDONS_CACHE_ID_ENABLED = "extensions.%ID%.getAddons.cache.enabled";
const PREF_UI_TYPE_HIDDEN = "extensions.ui.%TYPE%.hidden";
const PREF_UI_LASTCATEGORY = "extensions.ui.lastCategory";
const PREF_LEGACY_EXCEPTIONS = "extensions.legacy.exceptions";
const PREF_LEGACY_ENABLED = "extensions.legacy.enabled";

const LOADING_MSG_DELAY = 100;

const SEARCH_SCORE_MULTIPLIER_NAME = 2;
const SEARCH_SCORE_MULTIPLIER_DESCRIPTION = 2;

// Use integers so search scores are sortable by nsIXULSortService
const SEARCH_SCORE_MATCH_WHOLEWORD = 10;
const SEARCH_SCORE_MATCH_WORDBOUNDRY = 6;
const SEARCH_SCORE_MATCH_SUBSTRING = 3;

const UPDATES_RECENT_TIMESPAN = 2 * 24 * 3600000; // 2 days (in milliseconds)
const UPDATES_RELEASENOTES_TRANSFORMFILE = "chrome://mozapps/content/extensions/updateinfo.xsl";

const XMLURI_PARSE_ERROR = "http://www.mozilla.org/newlayout/xml/parsererror.xml"

var gViewDefault = "addons://discover/";

XPCOMUtils.defineLazyGetter(this, "extensionStylesheets", () => {
  const {ExtensionParent} = Cu.import("resource://gre/modules/ExtensionParent.jsm", {});
  return ExtensionParent.extensionStylesheets;
});

var gStrings = {};
XPCOMUtils.defineLazyServiceGetter(gStrings, "bundleSvc",
                                   "@mozilla.org/intl/stringbundle;1",
                                   "nsIStringBundleService");

XPCOMUtils.defineLazyGetter(gStrings, "brand", function() {
  return this.bundleSvc.createBundle("chrome://branding/locale/brand.properties");
});
XPCOMUtils.defineLazyGetter(gStrings, "ext", function() {
  return this.bundleSvc.createBundle("chrome://mozapps/locale/extensions/extensions.properties");
});
XPCOMUtils.defineLazyGetter(gStrings, "dl", function() {
  return this.bundleSvc.createBundle("chrome://mozapps/locale/downloads/downloads.properties");
});

XPCOMUtils.defineLazyGetter(gStrings, "brandShortName", function() {
  return this.brand.GetStringFromName("brandShortName");
});
XPCOMUtils.defineLazyGetter(gStrings, "appVersion", function() {
  return Services.appinfo.version;
});


XPCOMUtils.defineLazyPreferenceGetter(this, "legacyWarningExceptions",
                                      PREF_LEGACY_EXCEPTIONS, "",
                                      raw => raw.split(","));
XPCOMUtils.defineLazyPreferenceGetter(this, "legacyExtensionsEnabled",
                                      PREF_LEGACY_ENABLED, true,
                                      () => gLegacyView.refresh());

document.addEventListener("load", initialize, true);
window.addEventListener("unload", shutdown);

class MessageDispatcher {
  constructor(target) {
    this.listeners = new Map();
    this.target = target;
  }

  addMessageListener(name, handler) {
    if (!this.listeners.has(name)) {
      this.listeners.set(name, new Set());
    }

    this.listeners.get(name).add(handler);
  }

  removeMessageListener(name, handler) {
    if (this.listeners.has(name)) {
      this.listeners.get(name).delete(handler);
    }
  }

  sendAsyncMessage(name, data) {
    for (let handler of this.listeners.get(name) || new Set()) {
      Promise.resolve().then(() => {
        handler.receiveMessage({
          name,
          data,
          target: this.target,
        });
      });
    }
  }
}

/**
 * A mock FrameMessageManager global to allow frame scripts to run in
 * non-top-level, non-remote <browser>s as if they were top-level or
 * remote.
 *
 * @param {Element} browser
 *        A XUL <browser> element.
 */
class FakeFrameMessageManager {
  constructor(browser) {
    let dispatcher = new MessageDispatcher(browser);
    let frameDispatcher = new MessageDispatcher(null);

    let bind = (object, method) => object[method].bind(object);

    this.sendAsyncMessage = bind(frameDispatcher, "sendAsyncMessage");
    this.addMessageListener = bind(dispatcher, "addMessageListener");
    this.removeMessageListener = bind(dispatcher, "removeMessageListener");

    this.frame = {
      get content() {
        return browser.contentWindow;
      },

      get docShell() {
        return browser.docShell;
      },

      addEventListener: bind(browser, "addEventListener"),
      removeEventListener: bind(browser, "removeEventListener"),

      sendAsyncMessage: bind(dispatcher, "sendAsyncMessage"),
      addMessageListener: bind(frameDispatcher, "addMessageListener"),
      removeMessageListener: bind(frameDispatcher, "removeMessageListener"),
    }
  }

  loadFrameScript(url) {
    Services.scriptloader.loadSubScript(url, Object.create(this.frame));
  }
}

function promiseEvent(event, target, capture = false) {
  return new Promise(resolve => {
    target.addEventListener(event, resolve, {capture, once: true});
  });
}

var gPendingInitializations = 1;
Object.defineProperty(this, "gIsInitializing", {
  get: () => gPendingInitializations > 0
});

function initialize(event) {
  // XXXbz this listener gets _all_ load events for all nodes in the
  // document... but relies on not being called "too early".
  if (event.target instanceof XMLStylesheetProcessingInstruction) {
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

  let detailScreenshot = document.getElementById("detail-screenshot");
  detailScreenshot.addEventListener("load", function(event) {
    this.removeAttribute("loading");
  });
  detailScreenshot.addEventListener("error", function(event) {
    this.setAttribute("loading", "error");
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

  gViewController.initialize();
  gCategories.initialize();
  gHeader.initialize();
  gEventManager.initialize();
  Services.obs.addObserver(sendEMPong, "EM-ping");
  Services.obs.notifyObservers(window, "EM-loaded");

  // If the initial view has already been selected (by a call to loadView from
  // the above notifications) then bail out now
  if (gViewController.initialViewSelected)
    return;

  // If there is a history state to restore then use that
  if (window.history.state) {
    gViewController.updateState(window.history.state);
    return;
  }

  // Default to the last selected category
  var view = gCategories.node.value;

  // Allow passing in a view through the window arguments
  if ("arguments" in window && window.arguments.length > 0 &&
      window.arguments[0] !== null && "view" in window.arguments[0]) {
    view = window.arguments[0].view;
  }

  gViewController.loadInitialView(view);
}

function notifyInitialized() {
  if (!gIsInitializing)
    return;

  gPendingInitializations--;
  if (!gIsInitializing) {
    var event = document.createEvent("Events");
    event.initEvent("Initialized", true, true);
    document.dispatchEvent(event);
  }
}

function shutdown() {
  gCategories.shutdown();
  gSearchView.shutdown();
  gEventManager.shutdown();
  gViewController.shutdown();
  Services.obs.removeObserver(sendEMPong, "EM-ping");
}

function sendEMPong(aSubject, aTopic, aData) {
  Services.obs.notifyObservers(window, "EM-pong");
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

function isCorrectlySigned(aAddon) {
  // Add-ons without an "isCorrectlySigned" property are correctly signed as
  // they aren't the correct type for signing.
  return aAddon.isCorrectlySigned !== false;
}

function isDisabledUnsigned(addon) {
  return AddonSettings.REQUIRE_SIGNING && !isCorrectlySigned(addon);
}

function isLegacyExtension(addon) {
  let legacy = false;
  if (addon.type == "extension" && !addon.isWebExtension) {
    legacy = true;
  }
  if (addon.type == "theme") {
    // The logic here is kind of clunky but we want to mark complete
    // themes as legacy.  There's no explicit flag for complete
    // themes so explicitly check for new style themes (for which
    // isWebExtension is true) or lightweight themes (which have
    // ids that end with @personas.mozilla.org)
    legacy = !(addon.isWebExtension || addon.id.endsWith("@personas.mozilla.org"));
  }

  if (legacy && (addon.hidden || addon.signedState == AddonManager.SIGNEDSTATE_PRIVILEGED)) {
    legacy = false;
  }
  // Exceptions that can slip through above: the default theme plus
  // test pilot addons until we get SIGNEDSTATE_PRIVILEGED deployed.
  if (legacy && legacyWarningExceptions.includes(addon.id)) {
    legacy = false;
  }
  return legacy;
}

function isDiscoverEnabled() {
  if (Services.prefs.getPrefType(PREF_DISCOVERURL) == Services.prefs.PREF_INVALID)
    return false;

  try {
    if (!Services.prefs.getBoolPref(PREF_DISCOVER_ENABLED))
      return false;
  } catch (e) {}

  try {
    if (!Services.prefs.getBoolPref(PREF_XPI_ENABLED))
      return false;
  } catch (e) {}

  return true;
}

function getExperimentEndDate(aAddon) {
  if (!("@mozilla.org/browser/experiments-service;1" in Cc)) {
    return 0;
  }

  if (!aAddon.isActive) {
    return aAddon.endDate;
  }

  let experiment = Experiments.instance().getActiveExperiment();
  if (!experiment) {
    return 0;
  }

  return experiment.endDate;
}

/**
 * Obtain the main DOMWindow for the current context.
 */
function getMainWindow() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsIDocShellTreeItem)
               .rootTreeItem
               .QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindow);
}

function getBrowserElement() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDocShell)
               .chromeEventHandler;
}

/**
 * Obtain the DOMWindow that can open a preferences pane.
 *
 * This is essentially "get the browser chrome window" with the added check
 * that the supposed browser chrome window is capable of opening a preferences
 * pane.
 *
 * This may return null if we can't find the browser chrome window.
 */
function getMainWindowWithPreferencesPane() {
  let mainWindow = getMainWindow();
  if (mainWindow && "openAdvancedPreferences" in mainWindow) {
    return mainWindow;
  }
  return null;
}

/**
 * A wrapper around the HTML5 session history service that allows the browser
 * back/forward controls to work within the manager
 */
var HTML5History = {
  get index() {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .sessionHistory.index;
  },

  get canGoBack() {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .canGoBack;
  },

  get canGoForward() {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .canGoForward;
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
  }
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
    return (this.pos + 1) < this.states.length;
  },

  back() {
    if (this.pos == 0)
      throw Components.Exception("Cannot go back from this point");

    this.pos--;
    gViewController.updateState(this.states[this.pos]);
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  forward() {
    if ((this.pos + 1) >= this.states.length)
      throw Components.Exception("Cannot go forward from this point");

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
    if (this.pos == 0)
      throw Components.Exception("Cannot popState from this view");

    this.states.splice(this.pos, this.states.length);
    this.pos--;

    gViewController.updateState(this.states[this.pos]);
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  }
};

// If the window has a session history then use the HTML5 History wrapper
// otherwise use our fake history implementation
if (window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebNavigation)
          .sessionHistory) {
  var gHistory = HTML5History;
} else {
  gHistory = FakeHistory;
}

var gEventManager = {
  _listeners: {},
  _installListeners: [],

  initialize() {
    const ADDON_EVENTS = ["onEnabling", "onEnabled", "onDisabling",
                          "onDisabled", "onUninstalling", "onUninstalled",
                          "onInstalled", "onOperationCancelled",
                          "onUpdateAvailable", "onUpdateFinished",
                          "onCompatibilityUpdateAvailable",
                          "onPropertyChanged"];
    for (let evt of ADDON_EVENTS) {
      let event = evt;
      this[event] = (...aArgs) => this.delegateAddonEvent(event, aArgs);
    }

    const INSTALL_EVENTS = ["onNewInstall", "onDownloadStarted",
                            "onDownloadEnded", "onDownloadFailed",
                            "onDownloadProgress", "onDownloadCancelled",
                            "onInstallStarted", "onInstallEnded",
                            "onInstallFailed", "onInstallCancelled",
                            "onExternalInstall"];
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
        if (child.nodeName == "menuitem" &&
          gViewController.isCommandEnabled(child.command)) {
            countMenuItemsBeforeSep++;
        }
      }

      // Hide the separator if there are no visible menu items before it
      menuSep.hidden = (countMenuItemsBeforeSep == 0);

    });

    let addonTooltip = document.getElementById("addonitem-tooltip");
    addonTooltip.addEventListener("popupshowing", function() {
      let addonItem = addonTooltip.triggerNode;
      // The way the test triggers the tooltip the richlistitem is the
      // tooltipNode but in normal use it is the anonymous node. This allows
      // any case
      if (addonItem.localName != "richlistitem")
        addonItem = document.getBindingParent(addonItem);

      let tiptext = addonItem.getAttribute("name");

      if (addonItem.mAddon) {
        if (shouldShowVersionNumber(addonItem.mAddon)) {
          tiptext += " " + (addonItem.hasAttribute("upgrade") ? addonItem.mManualUpdate.version
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
    if (!(aAddonId in this._listeners))
      this._listeners[aAddonId] = [];
    else if (this._listeners[aAddonId].indexOf(aListener) != -1)
      return;
    this._listeners[aAddonId].push(aListener);
  },

  unregisterAddonListener(aListener, aAddonId) {
    if (!(aAddonId in this._listeners))
      return;
    var index = this._listeners[aAddonId].indexOf(aListener);
    if (index == -1)
      return;
    this._listeners[aAddonId].splice(index, 1);
  },

  registerInstallListener(aListener) {
    if (this._installListeners.indexOf(aListener) != -1)
      return;
    this._installListeners.push(aListener);
  },

  unregisterInstallListener(aListener) {
    var i = this._installListeners.indexOf(aListener);
    if (i == -1)
      return;
    this._installListeners.splice(i, 1);
  },

  delegateAddonEvent(aEvent, aParams) {
    var addon = aParams.shift();
    if (!(addon.id in this._listeners))
      return;

    var listeners = this._listeners[addon.id];
    for (let listener of listeners) {
      if (!(aEvent in listener))
        continue;
      try {
        listener[aEvent].apply(listener, aParams);
      } catch (e) {
        // this shouldn't be fatal
        Cu.reportError(e);
      }
    }
  },

  delegateInstallEvent(aEvent, aParams) {
    var existingAddon = aEvent == "onExternalInstall" ? aParams[1] : aParams[0].existingAddon;
    // If the install is an update then send the event to all listeners
    // registered for the existing add-on
    if (existingAddon)
      this.delegateAddonEvent(aEvent, [existingAddon].concat(aParams));

    for (let listener of this._installListeners) {
      if (!(aEvent in listener))
        continue;
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

    if (AddonManager.checkUpdateSecurityDefault &&
        !AddonManager.checkUpdateSecurity) {
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
    document.getElementById("utils-autoUpdateDefault")
            .setAttribute("checked", updateEnabled && autoUpdateDefault);

    document.getElementById("utils-resetAddonUpdatesToAutomatic").hidden = !autoUpdateDefault;
    document.getElementById("utils-resetAddonUpdatesToManual").hidden = autoUpdateDefault;
  },

  onCompatibilityModeChanged() {
    this.refreshGlobalWarning();
  },

  onCheckUpdateSecurityChanged() {
    this.refreshGlobalWarning();
  },

  onUpdateModeChanged() {
    this.refreshAutoUpdateDefault();
  }
};

function attachUpdateHandler(install) {
  if (!WEBEXT_PERMISSION_PROMPTS) {
    return;
  }

  install.promptHandler = (info) => {
    let oldPerms = info.existingAddon.userPermissions;
    if (!oldPerms) {
      // Updating from a legacy add-on, let it proceed
      return Promise.resolve();
    }

    let newPerms = info.addon.userPermissions;

    let difference = Extension.comparePermissions(oldPerms, newPerms);

    // If there are no new permissions, just proceed
    if (difference.origins.length == 0 && difference.permissions.length == 0) {
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      let subject = {
        wrappedJSObject: {
          target: getBrowserElement(),
          info: {
            type: "update",
            addon: info.addon,
            icon: info.addon.icon,
            permissions: difference,
            resolve,
            reject,
          },
        },
      };
      Services.obs.notifyObservers(subject, "webextension-permission-prompt");
    });
  };
}

var gViewController = {
  viewPort: null,
  currentViewId: "",
  currentViewObj: null,
  currentViewRequest: 0,
  viewObjects: {},
  viewChangeCallback: null,
  initialViewSelected: false,
  lastHistoryIndex: -1,

  initialize() {
    this.viewPort = document.getElementById("view-port");
    this.headeredViews = document.getElementById("headered-views");
    this.headeredViewsDeck = document.getElementById("headered-views-content");

    this.viewObjects["search"] = gSearchView;
    this.viewObjects["discover"] = gDiscoverView;
    this.viewObjects["list"] = gListView;
    this.viewObjects["legacy"] = gLegacyView;
    this.viewObjects["detail"] = gDetailView;
    this.viewObjects["updates"] = gUpdatesView;

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
    if (this.currentViewObj)
      this.currentViewObj.hide();
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
        if (gHistory.canGoBack)
          gHistory.back();
        else
          gViewController.replaceView(gViewDefault);
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
    return {type: viewType, param: decodeURIComponent(viewParam)};
  },

  get isLoading() {
    return !this.currentViewObj || this.currentViewObj.node.hasAttribute("loading");
  },

  loadView(aViewId) {
    var isRefresh = false;
    if (aViewId == this.currentViewId) {
      if (this.isLoading)
        return;
      if (!("refresh" in this.currentViewObj))
        return;
      if (!this.currentViewObj.canRefresh())
        return;
      isRefresh = true;
    }

    var state = {
      view: aViewId,
      previousView: this.currentViewId
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
    if (aViewId == this.currentViewId)
      return;

    var state = {
      view: aViewId,
      previousView: null
    };
    gHistory.replaceState(state);
    this.loadViewInternal(aViewId, null, state);
  },

  loadInitialView(aViewId) {
    var state = {
      view: aViewId,
      previousView: null
    };
    gHistory.replaceState(state);

    this.loadViewInternal(aViewId, null, state);
    this.initialViewSelected = true;
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

  loadViewInternal(aViewId, aPreviousView, aState) {
    var view = this.parseViewId(aViewId);

    if (!view.type || !(view.type in this.viewObjects))
      throw Components.Exception("Invalid view: " + view.type);

    var viewObj = this.viewObjects[view.type];
    if (!viewObj.node)
      throw Components.Exception("Root node doesn't exist for '" + view.type + "' view");

    if (this.currentViewObj && aViewId != aPreviousView) {
      try {
        let canHide = this.currentViewObj.hide();
        if (canHide === false)
          return;
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
    this.currentViewObj.node.focus();

    if (aViewId == aPreviousView)
      this.currentViewObj.refresh(view.param, ++this.currentViewRequest, aState);
    else
      this.currentViewObj.show(view.param, ++this.currentViewRequest, aState);
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
      }
    },

    cmd_forward: {
      isEnabled() {
        return gHistory.canGoForward;
      },
      doCommand() {
        gHistory.forward();
      }
    },

    cmd_focusSearch: {
      isEnabled: () => true,
      doCommand() {
        gHeader.focusSearchBox();
      }
    },

    cmd_restartApp: {
      isEnabled() {
        return true;
      },
      doCommand() {
        let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                         createInstance(Ci.nsISupportsPRBool);
        Services.obs.notifyObservers(cancelQuit, "quit-application-requested",
                                     "restart");
        if (cancelQuit.data)
          return; // somebody canceled our quit request

        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                         getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
      }
    },

    cmd_enableCheckCompatibility: {
      isEnabled() {
        return true;
      },
      doCommand() {
        AddonManager.checkCompatibility = true;
      }
    },

    cmd_enableUpdateSecurity: {
      isEnabled() {
        return true;
      },
      doCommand() {
        AddonManager.checkUpdateSecurity = true;
      }
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
      }
    },

    cmd_resetAddonAutoUpdate: {
      isEnabled() {
        return true;
      },
      doCommand() {
        AddonManager.getAllAddons(function(aAddonList) {
          for (let addon of aAddonList) {
            if ("applyBackgroundUpdates" in addon)
              addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;
          }
        });
      }
    },

    cmd_goToDiscoverPane: {
      isEnabled() {
        return gDiscoverView.enabled;
      },
      doCommand() {
        gViewController.loadView("addons://discover/");
      }
    },

    cmd_goToRecentUpdates: {
      isEnabled() {
        return true;
      },
      doCommand() {
        gViewController.loadView("addons://updates/recent");
      }
    },

    cmd_goToAvailableUpdates: {
      isEnabled() {
        return true;
      },
      doCommand() {
        gViewController.loadView("addons://updates/available");
      }
    },

    cmd_showItemDetails: {
      isEnabled(aAddon) {
        return !!aAddon && (gViewController.currentViewObj != gDetailView);
      },
      doCommand(aAddon, aScrollToPreferences) {
        gViewController.loadView("addons://detail/" +
                                 encodeURIComponent(aAddon.id) +
                                 (aScrollToPreferences ? "/preferences" : ""));
      }
    },

    cmd_findAllUpdates: {
      inProgress: false,
      isEnabled() {
        return !this.inProgress;
      },
      doCommand() {
        this.inProgress = true;
        gViewController.updateCommand("cmd_findAllUpdates");
        document.getElementById("updates-noneFound").hidden = true;
        document.getElementById("updates-progress").hidden = false;
        document.getElementById("updates-manualUpdatesFound-btn").hidden = true;

        var pendingChecks = 0;
        var numUpdated = 0;
        var numManualUpdates = 0;
        var restartNeeded = false;

        let updateStatus = () => {
          if (pendingChecks > 0)
            return;

          this.inProgress = false;
          gViewController.updateCommand("cmd_findAllUpdates");
          document.getElementById("updates-progress").hidden = true;
          gUpdatesView.maybeRefresh();

          Services.obs.notifyObservers(null, "EM-update-check-finished");

          if (numManualUpdates > 0 && numUpdated == 0) {
            document.getElementById("updates-manualUpdatesFound-btn").hidden = false;
            return;
          }

          if (numUpdated == 0) {
            document.getElementById("updates-noneFound").hidden = false;
            return;
          }

          if (restartNeeded) {
            document.getElementById("updates-downloaded").hidden = false;
            document.getElementById("updates-restart-btn").hidden = false;
          } else {
            document.getElementById("updates-installed").hidden = false;
          }
        }

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
            if (isPending(aInstall.existingAddon, "upgrade"))
              restartNeeded = true;
            updateStatus();
          }
        };

        var updateCheckListener = {
          onUpdateAvailable(aAddon, aInstall) {
            gEventManager.delegateAddonEvent("onUpdateAvailable",
                                             [aAddon, aInstall]);
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
            gEventManager.delegateAddonEvent("onUpdateFinished",
                                             [aAddon, aError]);
          }
        };

        AddonManager.getAddonsByTypes(null, function(aAddonList) {
          for (let addon of aAddonList) {
            if (addon.permissions & AddonManager.PERM_CAN_UPGRADE) {
              pendingChecks++;
              addon.findUpdates(updateCheckListener,
                                AddonManager.UPDATE_WHEN_USER_REQUESTED);
            }
          }

          if (pendingChecks == 0)
            updateStatus();
        });
      }
    },

    cmd_findItemUpdates: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        return hasPermission(aAddon, "upgrade");
      },
      doCommand(aAddon) {
        var listener = {
          onUpdateAvailable(aAddon, aInstall) {
            gEventManager.delegateAddonEvent("onUpdateAvailable",
                                             [aAddon, aInstall]);
            attachUpdateHandler(aInstall);
            if (AddonManager.shouldAutoUpdate(aAddon))
              aInstall.install();
          },
          onNoUpdateAvailable(aAddon) {
            gEventManager.delegateAddonEvent("onNoUpdateAvailable",
                                             [aAddon]);
          }
        };
        gEventManager.delegateAddonEvent("onCheckingUpdate", [aAddon]);
        aAddon.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
      }
    },

    cmd_showItemPreferences: {
      isEnabled(aAddon) {
        if (!aAddon ||
            (!aAddon.isActive && aAddon.type !== "plugin") ||
            !aAddon.optionsURL) {
          return false;
        }
        if (gViewController.currentViewObj == gDetailView &&
            (aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE ||
             aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE_BROWSER)) {
          return false;
        }
        if (aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE_INFO)
          return false;
        return true;
      },
      doCommand(aAddon) {
        if (hasInlineOptions(aAddon)) {
          gViewController.commands.cmd_showItemDetails.doCommand(aAddon, true);
          return;
        }
        var optionsURL = aAddon.optionsURL;
        if (aAddon.optionsType == AddonManager.OPTIONS_TYPE_TAB &&
            openOptionsInTab(optionsURL)) {
          return;
        }
        var windows = Services.wm.getEnumerator(null);
        while (windows.hasMoreElements()) {
          var win = windows.getNext();
          if (win.closed) {
            continue;
          }
          if (win.document.documentURI == optionsURL) {
            win.focus();
            return;
          }
        }
        var features = "chrome,titlebar,toolbar,centerscreen";
        try {
          var instantApply = Services.prefs.getBoolPref("browser.preferences.instantApply");
          features += instantApply ? ",dialog=no" : ",modal";
        } catch (e) {
          features += ",modal";
        }
        openDialog(optionsURL, "", features);
      }
    },

    cmd_showItemAbout: {
      isEnabled(aAddon) {
        // XXXunf This may be applicable to install items too. See bug 561260
        return !!aAddon;
      },
      doCommand(aAddon) {
        var aboutURL = aAddon.aboutURL;
        if (aboutURL)
          openDialog(aboutURL, "", "chrome,centerscreen,modal", aAddon);
        else
          openDialog("chrome://mozapps/content/extensions/about.xul",
                     "", "chrome,centerscreen,modal", aAddon);
      }
    },

    cmd_enableItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        let addonType = AddonManager.addonTypes[aAddon.type];
        return (!(addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) &&
                hasPermission(aAddon, "enable"));
      },
      doCommand(aAddon) {
        if (aAddon.isWebExtension && !aAddon.seen && WEBEXT_PERMISSION_PROMPTS) {
          let perms = aAddon.userPermissions;
          if (perms.origins.length > 0 || perms.permissions.length > 0) {
            let subject = {
              wrappedJSObject: {
                target: getBrowserElement(),
                info: {
                  type: "sideload",
                  addon: aAddon,
                  icon: aAddon.iconURL,
                  permissions: perms,
                  resolve() { aAddon.userDisabled = false },
                  reject() {},
                },
              },
            };
            Services.obs.notifyObservers(subject, "webextension-permission-prompt");
            return;
          }
        }
        aAddon.userDisabled = false;
      },
      getTooltip(aAddon) {
        if (!aAddon)
          return "";
        if (aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_ENABLE)
          return gStrings.ext.GetStringFromName("enableAddonRestartRequiredTooltip");
        return gStrings.ext.GetStringFromName("enableAddonTooltip");
      }
    },

    cmd_disableItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        let addonType = AddonManager.addonTypes[aAddon.type];
        return (!(addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) &&
                hasPermission(aAddon, "disable"));
      },
      doCommand(aAddon) {
        aAddon.userDisabled = true;
      },
      getTooltip(aAddon) {
        if (!aAddon)
          return "";
        if (aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_DISABLE)
          return gStrings.ext.GetStringFromName("disableAddonRestartRequiredTooltip");
        return gStrings.ext.GetStringFromName("disableAddonTooltip");
      }
    },

    cmd_installItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        return aAddon.install && aAddon.install.state == AddonManager.STATE_AVAILABLE;
      },
      doCommand(aAddon) {
        function doInstall() {
          gViewController.currentViewObj.getListItemForID(aAddon.id)._installStatus.installRemote();
        }

        if (gViewController.currentViewObj == gDetailView)
          gViewController.popState(doInstall);
        else
          doInstall();
      }
    },

    cmd_purchaseItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        return !!aAddon.purchaseURL;
      },
      doCommand(aAddon) {
        openURL(aAddon.purchaseURL);
      }
    },

    cmd_uninstallItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        return hasPermission(aAddon, "uninstall");
      },
      doCommand(aAddon) {
        if (gViewController.currentViewObj != gDetailView) {
          aAddon.uninstall();
          return;
        }

        gViewController.popState(function() {
          gViewController.currentViewObj.getListItemForID(aAddon.id).uninstall();
        });
      },
      getTooltip(aAddon) {
        if (!aAddon)
          return "";
        if (aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL)
          return gStrings.ext.GetStringFromName("uninstallAddonRestartRequiredTooltip");
        return gStrings.ext.GetStringFromName("uninstallAddonTooltip");
      }
    },

    cmd_cancelUninstallItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        return isPending(aAddon, "uninstall");
      },
      doCommand(aAddon) {
        aAddon.cancelUninstall();
      }
    },

    cmd_installFromFile: {
      isEnabled() {
        return true;
      },
      doCommand() {
        const nsIFilePicker = Ci.nsIFilePicker;
        var fp = Cc["@mozilla.org/filepicker;1"]
                   .createInstance(nsIFilePicker);
        fp.init(window,
                gStrings.ext.GetStringFromName("installFromFile.dialogTitle"),
                nsIFilePicker.modeOpenMultiple);
        try {
          fp.appendFilter(gStrings.ext.GetStringFromName("installFromFile.filterName"),
                          "*.xpi;*.jar;*.zip");
          fp.appendFilters(nsIFilePicker.filterAll);
        } catch (e) { }

        fp.open(result => {
          if (result != nsIFilePicker.returnOK)
            return;

          let browser = getBrowserElement();
          let files = fp.files;
          while (files.hasMoreElements()) {
            let file = files.getNext();
            AddonManager.getInstallForFile(file, install => {
              AddonManager.installAddonFromAOM(browser, document.documentURIObject, install);
            });
          }
        });
      }
    },

    cmd_debugAddons: {
      isEnabled() {
        return true;
      },
      doCommand() {
        let mainWindow = getMainWindow();
        if ("switchToTabHavingURI" in mainWindow) {
          mainWindow.switchToTabHavingURI("about:debugging#addons", true);
        }
      },
    },

    cmd_cancelOperation: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        return aAddon.pendingOperations != AddonManager.PENDING_NONE;
      },
      doCommand(aAddon) {
        if (isPending(aAddon, "install")) {
          aAddon.install.cancel();
        } else if (isPending(aAddon, "upgrade")) {
          aAddon.pendingUpgrade.install.cancel();
        } else if (isPending(aAddon, "uninstall")) {
          aAddon.cancelUninstall();
        } else if (isPending(aAddon, "enable")) {
          aAddon.userDisabled = true;
        } else if (isPending(aAddon, "disable")) {
          aAddon.userDisabled = false;
        }
      }
    },

    cmd_contribute: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        return ("contributionURL" in aAddon && aAddon.contributionURL);
      },
      doCommand(aAddon) {
        openURL(aAddon.contributionURL);
      }
    },

    cmd_askToActivateItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        let addonType = AddonManager.addonTypes[aAddon.type];
        return ((addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) &&
                hasPermission(aAddon, "ask_to_activate"));
      },
      doCommand(aAddon) {
        aAddon.userDisabled = AddonManager.STATE_ASK_TO_ACTIVATE;
      }
    },

    cmd_alwaysActivateItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        let addonType = AddonManager.addonTypes[aAddon.type];
        return ((addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) &&
                hasPermission(aAddon, "enable"));
      },
      doCommand(aAddon) {
        aAddon.userDisabled = false;
      }
    },

    cmd_neverActivateItem: {
      isEnabled(aAddon) {
        if (!aAddon)
          return false;
        let addonType = AddonManager.addonTypes[aAddon.type];
        return ((addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) &&
                hasPermission(aAddon, "disable"));
      },
      doCommand(aAddon) {
        aAddon.userDisabled = true;
      }
    },

    cmd_experimentsLearnMore: {
      isEnabled() {
        let mainWindow = getMainWindow();
        return mainWindow && "switchToTabHavingURI" in mainWindow;
      },
      doCommand() {
        let url = Services.prefs.getCharPref("toolkit.telemetry.infoURL");
        openOptionsInTab(url);
      },
    },

    cmd_experimentsOpenTelemetryPreferences: {
      isEnabled() {
        return !!getMainWindowWithPreferencesPane();
      },
      doCommand() {
        let mainWindow = getMainWindowWithPreferencesPane();
        // The advanced subpanes are only supported in the old organization, which will
        // be removed by bug 1349689.
        if (Preferences.get("browser.preferences.useOldOrganization")) {
          mainWindow.openAdvancedPreferences("dataChoicesTab", {origin: "experimentsOpenPref"});
        } else {
          mainWindow.openPreferences("paneAdvanced", {origin: "experimentsOpenPref"});
        }
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
  },

  supportsCommand(aCommand) {
    return (aCommand in this.commands);
  },

  isCommandEnabled(aCommand) {
    if (!this.supportsCommand(aCommand))
      return false;
    var addon = this.currentViewObj.getSelectedAddon();
    return this.commands[aCommand].isEnabled(addon);
  },

  updateCommands() {
    // wait until the view is initialized
    if (!this.currentViewObj)
      return;
    var addon = this.currentViewObj.getSelectedAddon();
    for (let commandId in this.commands)
      this.updateCommand(commandId, addon);
  },

  updateCommand(aCommandId, aAddon) {
    if (typeof aAddon == "undefined")
      aAddon = this.currentViewObj.getSelectedAddon();
    var cmd = this.commands[aCommandId];
    var cmdElt = document.getElementById(aCommandId);
    cmdElt.setAttribute("disabled", !cmd.isEnabled(aAddon));
    if ("getTooltip" in cmd) {
      let tooltip = cmd.getTooltip(aAddon);
      if (tooltip)
        cmdElt.setAttribute("tooltiptext", tooltip);
      else
        cmdElt.removeAttribute("tooltiptext");
    }
  },

  doCommand(aCommand, aAddon) {
    if (!this.supportsCommand(aCommand))
      return;
    var cmd = this.commands[aCommand];
    if (!aAddon)
      aAddon = this.currentViewObj.getSelectedAddon();
    if (!cmd.isEnabled(aAddon))
      return;
    cmd.doCommand(aAddon);
  },

  onEvent() {}
};

function hasInlineOptions(aAddon) {
  return (aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE ||
          aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE_BROWSER ||
          aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE_INFO);
}

function openOptionsInTab(optionsURL) {
  let mainWindow = getMainWindow();
  if ("switchToTabHavingURI" in mainWindow) {
    mainWindow.switchToTabHavingURI(optionsURL, true);
    return true;
  }
  return false;
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
  if (!aAddon.version)
    return false;

  // The version number is hidden for experiments.
  if (aAddon.type == "experiment")
    return false;

  // The version number is hidden for lightweight themes.
  if (aAddon.type == "theme")
    return !/@personas\.mozilla\.org$/.test(aAddon.id);

  return true;
}

function createItem(aObj, aIsInstall, aIsRemote) {
  let item = document.createElement("richlistitem");

  item.setAttribute("class", "addon addon-view");
  item.setAttribute("name", aObj.name);
  item.setAttribute("type", aObj.type);
  item.setAttribute("remote", !!aIsRemote);

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

  if (aObj.type == "experiment") {
    item.endDate = getExperimentEndDate(aObj);
  }

  return item;
}

function sortElements(aElements, aSortBy, aAscending) {
  // aSortBy is an Array of attributes to sort by, in decending
  // order of priority.

  const DATE_FIELDS = ["updateDate"];
  const NUMERIC_FIELDS = ["size", "relevancescore", "purchaseAmount"];

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

  const UISTATE_ORDER = ["enabled", "askToActivate", "pendingDisable",
                         "pendingUninstall", "disabled"];

  function dateCompare(a, b) {
    var aTime = a.getTime();
    var bTime = b.getTime();
    if (aTime < bTime)
      return -1;
    if (aTime > bTime)
      return 1;
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
    if (!aAscending)
      [a, b] = [b, a];

    return (UISTATE_ORDER.indexOf(a) - UISTATE_ORDER.indexOf(b));
  }

  function getValue(aObj, aKey) {
    if (!aObj)
      return null;

    if (aObj.hasAttribute(aKey))
      return aObj.getAttribute(aKey);

    var addon = aObj.mAddon || aObj.mInstall;
    var addonType = aObj.mAddon && AddonManager.addonTypes[aObj.mAddon.type];

    if (!addon)
      return null;

    if (aKey == "uiState") {
      if (addon.pendingOperations == AddonManager.PENDING_DISABLE)
        return "pendingDisable";
      if (addon.pendingOperations == AddonManager.PENDING_UNINSTALL)
        return "pendingUninstall";
      if (!addon.isActive &&
          (addon.pendingOperations != AddonManager.PENDING_ENABLE &&
           addon.pendingOperations != AddonManager.PENDING_INSTALL))
        return "disabled";
      if (addonType && (addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) &&
          addon.userDisabled == AddonManager.STATE_ASK_TO_ACTIVATE)
        return "askToActivate";
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

    if (sortBy == "uiState")
      aSortFuncs[i] = uiStateCompare;
    else if (DATE_FIELDS.indexOf(sortBy) != -1)
      aSortFuncs[i] = dateCompare;
    else if (NUMERIC_FIELDS.indexOf(sortBy) != -1)
      aSortFuncs[i] = numberCompare;
  }


  aElements.sort(function(a, b) {
    if (!aAscending)
      [a, b] = [b, a];

    for (let i = 0; i < aSortFuncs.length; i++) {
      var sortBy = aSortBy[i];
      var aValue = getValue(a, sortBy);
      var bValue = getValue(b, sortBy);

      if (!aValue && !bValue)
        return 0;
      if (!aValue)
        return -1;
      if (!bValue)
        return 1;
      if (aValue != bValue) {
        var result = aSortFuncs[i](aValue, bValue);

        if (result != 0)
          return result;
      }
    }

    // If we got here, then all values of a and b
    // must have been equal.
    return 0;

  });
}

function sortList(aList, aSortBy, aAscending) {
  var elements = Array.slice(aList.childNodes, 0);
  sortElements(elements, [aSortBy], aAscending);

  while (aList.listChild)
    aList.removeChild(aList.lastChild);

  for (let element of elements)
    aList.appendChild(element);
}

function getAddonsAndInstalls(aType, aCallback) {
  let addons = null, installs = null;
  let types = (aType != null) ? [aType] : null;

  AddonManager.getAddonsByTypes(types, function(aAddonsList) {
    addons = aAddonsList.filter(a => !a.hidden);
    if (installs != null)
      aCallback(addons, installs);
  });

  AddonManager.getInstallsByTypes(types, function(aInstallsList) {
    // skip over upgrade installs and non-active installs
    installs = aInstallsList.filter(function(aInstall) {
      return !(aInstall.existingAddon ||
               aInstall.state == AddonManager.STATE_AVAILABLE);
    });

    if (addons != null)
      aCallback(addons, installs)
  });
}

function doPendingUninstalls(aListBox) {
  // Uninstalling add-ons can mutate the list so find the add-ons first then
  // uninstall them
  var items = [];
  var listitem = aListBox.firstChild;
  while (listitem) {
    if (listitem.getAttribute("pending") == "uninstall" &&
        !(listitem.opRequiresRestart("UNINSTALL")))
      items.push(listitem.mAddon);
    listitem = listitem.nextSibling;
  }

  for (let addon of items)
    addon.uninstall();
}

var gCategories = {
  node: null,
  _search: null,

  initialize() {
    this.node = document.getElementById("categories");
    this._search = this.get("addons://search/");

    var types = AddonManager.addonTypes;
    for (var type in types)
      this.onTypeAdded(types[type]);

    AddonManager.addTypeListener(this);

    // eslint-disable-next-line mozilla/use-default-preference-values
    try {
      this.node.value = Services.prefs.getCharPref(PREF_UI_LASTCATEGORY);
    } catch (e) { }

    // If there was no last view or no existing category matched the last view
    // then the list will default to selecting the search category and we never
    // want to show that as the first view so switch to the default category
    if (!this.node.selectedItem || this.node.selectedItem == this._search)
      this.node.value = gViewDefault;

    this.node.addEventListener("select", () => {
      this.maybeHideSearch();
      gViewController.loadView(this.node.selectedItem.value);
    });

    this.node.addEventListener("click", (aEvent) => {
      var selectedItem = this.node.selectedItem;
      if (aEvent.target.localName == "richlistitem" &&
          aEvent.target == selectedItem) {
        var viewId = selectedItem.value;

        if (gViewController.parseViewId(viewId).type == "search") {
          viewId += encodeURIComponent(gHeader.searchQuery);
        }

        gViewController.loadView(viewId);
      }
    });
  },

  shutdown() {
    AddonManager.removeTypeListener(this);
  },

  _insertCategory(aId, aName, aView, aPriority, aStartHidden) {
    // If this category already exists then don't re-add it
    if (document.getElementById("category-" + aId))
      return;

    var category = document.createElement("richlistitem");
    category.setAttribute("id", "category-" + aId);
    category.setAttribute("value", aView);
    category.setAttribute("class", "category");
    category.setAttribute("name", aName);
    category.setAttribute("tooltiptext", aName);
    category.setAttribute("priority", aPriority);
    category.setAttribute("hidden", aStartHidden);

    var node;
    for (node of this.node.children) {
      var nodePriority = parseInt(node.getAttribute("priority"));
      // If the new type's priority is higher than this one then this is the
      // insertion point
      if (aPriority < nodePriority)
        break;
      // If the new type's priority is lower than this one then this is isn't
      // the insertion point
      if (aPriority > nodePriority)
        continue;
      // If the priorities are equal and the new type's name is earlier
      // alphabetically then this is the insertion point
      if (String(aName).localeCompare(node.getAttribute("name")) < 0)
        break;
    }

    this.node.insertBefore(category, node);
  },

  _removeCategory(aId) {
    var category = document.getElementById("category-" + aId);
    if (!category)
      return;

    // If this category is currently selected then switch to the default view
    if (this.node.selectedItem == category)
      gViewController.replaceView(gViewDefault);

    this.node.removeChild(category);
  },

  onTypeAdded(aType) {
    // Ignore types that we don't have a view object for
    if (!(aType.viewType in gViewController.viewObjects))
      return;

    var aViewId = "addons://" + aType.viewType + "/" + aType.id;

    var startHidden = false;
    if (aType.flags & AddonManager.TYPE_UI_HIDE_EMPTY) {
      var prefName = PREF_UI_TYPE_HIDDEN.replace("%TYPE%", aType.id);
      startHidden = Services.prefs.getBoolPref(prefName, true);

      gPendingInitializations++;
      getAddonsAndInstalls(aType.id, (aAddonsList, aInstallsList) => {
        var hidden = (aAddonsList.length == 0 && aInstallsList.length == 0);
        var item = this.get(aViewId);

        // Don't load view that is becoming hidden
        if (hidden && aViewId == gViewController.currentViewId)
          gViewController.loadView(gViewDefault);

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

          onExternalInstall(aAddon, aExistingAddon, aRequiresRestart) {
            this._maybeShowCategory(aAddon);
          },

          _maybeShowCategory: aAddon => {
            if (aType.id == aAddon.type) {
              this.get(aViewId).hidden = false;
              Services.prefs.setBoolPref(prefName, false);
              gEventManager.unregisterInstallListener(this);
            }
          }
        });

        notifyInitialized();
      });
    }

    this._insertCategory(aType.id, aType.name, aViewId, aType.uiPriority,
                         startHidden);
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

    if (this.node.selectedItem &&
        this.node.selectedItem.value == aId) {
      this.node.selectedItem.hidden = false;
      this.node.selectedItem.disabled = false;
      return;
    }

    var item;
    if (view.type == "search")
      item = this._search;
    else
      item = this.get(aId);

    if (item) {
      item.hidden = false;
      item.disabled = false;
      this.node.suppressOnSelect = true;
      this.node.selectedItem = item;
      this.node.suppressOnSelect = false;
      this.node.ensureElementIsVisible(item);

      this.maybeHideSearch();
    }
  },

  get(aId) {
    var items = document.getElementsByAttribute("value", aId);
    if (items.length)
      return items[0];
    return null;
  },

  setBadge(aId, aCount) {
    let item = this.get(aId);
    if (item)
      item.badgeCount = aCount;
  },

  maybeHideSearch() {
    var view = gViewController.parseViewId(this.node.selectedItem.value);
    this._search.disabled = view.type != "search";
  }
};


var gHeader = {
  _search: null,
  _dest: "",

  initialize() {
    this._search = document.getElementById("header-search");

    this._search.addEventListener("command", function(aEvent) {
      var query = aEvent.target.value;
      if (query.length == 0)
        return;

      gViewController.loadView("addons://search/" + encodeURIComponent(query));
    });

    function updateNavButtonVisibility() {
      var shouldShow = gHeader.shouldShowNavButtons;
      document.getElementById("back-btn").hidden = !shouldShow;
      document.getElementById("forward-btn").hidden = !shouldShow;
    }

    window.addEventListener("focus", function(aEvent) {
      if (aEvent.target == window)
        updateNavButtonVisibility();
    });

    updateNavButtonVisibility();
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
    var docshellItem = window.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIWebNavigation)
                             .QueryInterface(Ci.nsIDocShellTreeItem);

    // If there is no outer frame then make the buttons visible
    if (docshellItem.rootTreeItem == docshellItem)
      return true;

    var outerWin = docshellItem.rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
                                            .getInterface(Ci.nsIDOMWindow);
    var outerDoc = outerWin.document;
    var node = outerDoc.getElementById("back-button");
    // If the outer frame has no back-button then make the buttons visible
    if (!node)
      return true;

    // If the back-button or any of its parents are hidden then make the buttons
    // visible
    while (node != outerDoc) {
      var style = outerWin.getComputedStyle(node);
      if (style.display == "none")
        return true;
      if (style.visibility != "visible")
        return true;
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

  initialize() {
    this.enabled = isDiscoverEnabled();
    if (!this.enabled) {
      gCategories.get("addons://discover/").hidden = true;
      return;
    }

    this.node = document.getElementById("discover-view");
    this._loading = document.getElementById("discover-loading");
    this._error = document.getElementById("discover-error");
    this._browser = document.getElementById("discover-browser");

    let compatMode = "normal";
    if (!AddonManager.checkCompatibility)
      compatMode = "ignore";
    else if (AddonManager.strictCompatibility)
      compatMode = "strict";

    var url = Services.prefs.getCharPref(PREF_DISCOVERURL);
    url = url.replace("%COMPATIBILITY_MODE%", compatMode);
    url = Services.urlFormatter.formatURL(url);

    let setURL = (aURL) => {
      try {
        this.homepageURL = Services.io.newURI(aURL);
      } catch (e) {
        this.showError();
        notifyInitialized();
        return;
      }

      this._browser.homePage = this.homepageURL.spec;
      this._browser.addProgressListener(this);

      if (this.loaded)
        this._loadURL(this.homepageURL.spec, false, notifyInitialized);
      else
        notifyInitialized();
    }

    if (Services.prefs.getBoolPref(PREF_GETADDONS_CACHE_ENABLED) == false) {
      setURL(url);
      return;
    }

    gPendingInitializations++;
    AddonManager.getAllAddons(function(aAddons) {
      var list = {};
      for (let addon of aAddons) {
        var prefName = PREF_GETADDONS_CACHE_ID_ENABLED.replace("%ID%",
                                                               addon.id);
        try {
          if (!Services.prefs.getBoolPref(prefName))
            continue;
        } catch (e) { }
        list[addon.id] = {
          name: addon.name,
          version: addon.version,
          type: addon.type,
          userDisabled: addon.userDisabled,
          isCompatible: addon.isCompatible,
          isBlocklisted: addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED
        }
      }

      setURL(url + "#" + JSON.stringify(list));
    });
  },

  destroy() {
    try {
      this._browser.removeProgressListener(this);
    } catch (e) {
      // Ignore the case when the listener wasn't already registered
    }
  },

  show(aParam, aRequest, aState, aIsRefresh) {
    gViewController.updateCommands();

    // If we're being told to load a specific URL then just do that
    if (aState && "url" in aState) {
      this.loaded = true;
      this._loadURL(aState.url);
    }

    // If the view has loaded before and still at the homepage (if refreshing),
    // and the error page is not visible then there is nothing else to do
    if (this.loaded && this.node.selectedPanel != this._error &&
        (!aIsRefresh || (this._browser.currentURI &&
         this._browser.currentURI.spec == this._browser.homePage))) {
      gViewController.notifyViewChanged();
      return;
    }

    this.loaded = true;

    // No homepage means initialization isn't complete, the browser will get
    // loaded once initialization is complete
    if (!this.homepageURL) {
      this._loadListeners.push(gViewController.notifyViewChanged.bind(gViewController));
      return;
    }

    this._loadURL(this.homepageURL.spec, aIsRefresh,
                  gViewController.notifyViewChanged.bind(gViewController));
  },

  canRefresh() {
    if (this._browser.currentURI &&
        this._browser.currentURI.spec == this._browser.homePage)
      return false;
    return true;
  },

  refresh(aParam, aRequest, aState) {
    this.show(aParam, aRequest, aState, true);
  },

  hide() { },

  showError() {
    this.node.selectedPanel = this._error;
  },

  _loadURL(aURL, aKeepHistory, aCallback) {
    if (this._browser.currentURI.spec == aURL) {
      if (aCallback)
        aCallback();
      return;
    }

    if (aCallback)
      this._loadListeners.push(aCallback);

    var flags = 0;
    if (!aKeepHistory)
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;

    this._browser.loadURIWithFlags(aURL, flags);
  },

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    // Ignore the about:blank load
    if (aLocation.spec == "about:blank")
      return;

    // When using the real session history the inner-frame will update the
    // session history automatically, if using the fake history though it must
    // be manually updated
    if (gHistory == FakeHistory) {
      var docshell = aWebProgress.QueryInterface(Ci.nsIDocShell);

      var state = {
        view: "addons://discover/",
        url: aLocation.spec
      };

      var replaceHistory = Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY << 16;
      if (docshell.loadType & replaceHistory)
        gHistory.replaceState(state);
      else
        gHistory.pushState(state);
      gViewController.lastHistoryIndex = gHistory.index;
    }

    gViewController.updateCommands();

    // If the hostname is the same as the new location's host and either the
    // default scheme is insecure or the new location is secure then continue
    // with the load
    if (aLocation.host == this.homepageURL.host &&
        (!this.homepageURL.schemeIs("https") || aLocation.schemeIs("https")))
      return;

    // Canceling the request will send an error to onStateChange which will show
    // the error page
    aRequest.cancel(Components.results.NS_BINDING_ABORTED);
  },

  onSecurityChange(aWebProgress, aRequest, aState) {
    // Don't care about security if the page is not https
    if (!this.homepageURL.schemeIs("https"))
      return;

    // If the request was secure then it is ok
    if (aState & Ci.nsIWebProgressListener.STATE_IS_SECURE)
      return;

    // Canceling the request will send an error to onStateChange which will show
    // the error page
    aRequest.cancel(Components.results.NS_BINDING_ABORTED);
  },

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let transferStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                        Ci.nsIWebProgressListener.STATE_IS_REQUEST |
                        Ci.nsIWebProgressListener.STATE_TRANSFERRING;
    // Once transferring begins show the content
    if ((aStateFlags & transferStart) === transferStart)
      this.node.selectedPanel = this._browser;

    // Only care about the network events
    if (!(aStateFlags & (Ci.nsIWebProgressListener.STATE_IS_NETWORK)))
      return;

    // If this is the start of network activity then show the loading page
    if (aStateFlags & (Ci.nsIWebProgressListener.STATE_START))
      this.node.selectedPanel = this._loading;

    // Ignore anything except stop events
    if (!(aStateFlags & (Ci.nsIWebProgressListener.STATE_STOP)))
      return;

    // Consider the successful load of about:blank as still loading
    if (aRequest instanceof Ci.nsIChannel && aRequest.URI.spec == "about:blank")
      return;

    // If there was an error loading the page or the new hostname is not the
    // same as the default hostname or the default scheme is secure and the new
    // scheme is insecure then show the error page
    const NS_ERROR_PARSED_DATA_CACHED = 0x805D0021;
    if (!(Components.isSuccessCode(aStatus) || aStatus == NS_ERROR_PARSED_DATA_CACHED) ||
        (aRequest && aRequest instanceof Ci.nsIHttpChannel && !aRequest.requestSucceeded)) {
      this.showError();
    } else {
      // Got a successful load, make sure the browser is visible
      this.node.selectedPanel = this._browser;
      gViewController.updateCommands();
    }

    var listeners = this._loadListeners;
    this._loadListeners = [];

    for (let listener of listeners)
      listener();
  },

  onProgressChange() { },
  onStatusChange() { },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  getSelectedAddon() {
    return null;
  }
};


var gCachedAddons = {};

var gSearchView = {
  node: null,
  _filter: null,
  _sorters: null,
  _loading: null,
  _listBox: null,
  _emptyNotice: null,
  _allResultsLink: null,
  _lastQuery: null,
  _lastRemoteTotal: 0,
  _pendingSearches: 0,

  initialize() {
    this.node = document.getElementById("search-view");
    this._filter = document.getElementById("search-filter-radiogroup");
    this._sorters = document.getElementById("search-sorters");
    this._sorters.handler = this;
    this._loading = document.getElementById("search-loading");
    this._listBox = document.getElementById("search-list");
    this._emptyNotice = document.getElementById("search-list-empty");
    this._allResultsLink = document.getElementById("search-allresults-link");

    if (!AddonManager.isInstallEnabled("application/x-xpinstall"))
      this._filter.hidden = true;

    this._listBox.addEventListener("keydown", aEvent => {
      if (aEvent.keyCode == aEvent.DOM_VK_RETURN) {
        var item = this._listBox.selectedItem;
        if (item)
          item.showInDetailView();
      }
    });

    this._filter.addEventListener("command", () => this.updateView());
  },

  shutdown() {
    if (AddonRepository.isSearching)
      AddonRepository.cancelSearch();
  },

  get isSearching() {
    return this._pendingSearches > 0;
  },

  show(aQuery, aRequest) {
    gEventManager.registerInstallListener(this);

    this.showEmptyNotice(false);
    this.showAllResultsLink(0);
    this.showLoading(true);
    this._sorters.showprice = false;

    gHeader.searchQuery = aQuery;
    aQuery = aQuery.trim().toLocaleLowerCase();
    if (this._lastQuery == aQuery) {
      this.updateView();
      gViewController.notifyViewChanged();
      return;
    }
    this._lastQuery = aQuery;

    if (AddonRepository.isSearching)
      AddonRepository.cancelSearch();

    while (this._listBox.firstChild.localName == "richlistitem")
      this._listBox.firstChild.remove();

    gCachedAddons = {};
    this._pendingSearches = 2;
    this._sorters.setSort("relevancescore", false);

    var elements = [];

    let createSearchResults = (aObjsList, aIsInstall, aIsRemote) => {
      for (let index in aObjsList) {
        let obj = aObjsList[index];
        let score = aObjsList.length - index;
        if (!aIsRemote && aQuery.length > 0) {
          score = this.getMatchScore(obj, aQuery);
          if (score == 0)
            continue;
        }

        let item = createItem(obj, aIsInstall, aIsRemote);
        item.setAttribute("relevancescore", score);
        if (aIsRemote) {
          gCachedAddons[obj.id] = obj;
          if (obj.purchaseURL)
            this._sorters.showprice = true;
        }

        elements.push(item);
      }
    }

    let finishSearch = (createdCount) => {
      if (elements.length > 0) {
        sortElements(elements, [this._sorters.sortBy], this._sorters.ascending);
        for (let element of elements)
          this._listBox.insertBefore(element, this._listBox.lastChild);
        this.updateListAttributes();
      }

      this._pendingSearches--;
      this.updateView();

      if (!this.isSearching)
        gViewController.notifyViewChanged();
    }

    getAddonsAndInstalls(null, function(aAddons, aInstalls) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      createSearchResults(aAddons, false, false);
      createSearchResults(aInstalls, true, false);
      finishSearch();
    });

    var maxRemoteResults = Services.prefs.getIntPref(PREF_MAXRESULTS, 0);

    if (maxRemoteResults <= 0) {
      finishSearch(0);
      return;
    }

    AddonRepository.searchAddons(aQuery, maxRemoteResults, {
      searchFailed: () => {
        if (gViewController && aRequest != gViewController.currentViewRequest)
          return;

        this._lastRemoteTotal = 0;

        // XXXunf Better handling of AMO search failure. See bug 579502
        finishSearch(0); // Silently fail
      },

      searchSucceeded: (aAddonsList, aAddonCount, aTotalResults) => {
        if (gViewController && aRequest != gViewController.currentViewRequest)
          return;

        if (aTotalResults > maxRemoteResults)
          this._lastRemoteTotal = aTotalResults;
        else
          this._lastRemoteTotal = 0;

        var createdCount = createSearchResults(aAddonsList, false, true);
        finishSearch(createdCount);
      }
    });
  },

  showLoading(aLoading) {
    this._loading.hidden = !aLoading;
    this._listBox.hidden = aLoading;
  },

  updateView() {
    var showLocal = this._filter.value == "local";

    if (!showLocal && !AddonManager.isInstallEnabled("application/x-xpinstall"))
      showLocal = true;

    this._listBox.setAttribute("local", showLocal);
    this._listBox.setAttribute("remote", !showLocal);

    this.showLoading(this.isSearching && !showLocal);
    if (!this.isSearching) {
      var isEmpty = true;
      var results = this._listBox.getElementsByTagName("richlistitem");
      for (let result of results) {
        var isRemote = (result.getAttribute("remote") == "true");
        if ((isRemote && !showLocal) || (!isRemote && showLocal)) {
          isEmpty = false;
          break;
        }
      }

      this.showEmptyNotice(isEmpty);
      this.showAllResultsLink(this._lastRemoteTotal);
    }

    gViewController.updateCommands();
  },

  hide() {
    gEventManager.unregisterInstallListener(this);
    doPendingUninstalls(this._listBox);
  },

  getMatchScore(aObj, aQuery) {
    var score = 0;
    score += this.calculateMatchScore(aObj.name, aQuery,
                                      SEARCH_SCORE_MULTIPLIER_NAME);
    score += this.calculateMatchScore(aObj.description, aQuery,
                                      SEARCH_SCORE_MULTIPLIER_DESCRIPTION);
    return score;
  },

  calculateMatchScore(aStr, aQuery, aMultiplier) {
    var score = 0;
    if (!aStr || aQuery.length == 0)
      return score;

    aStr = aStr.trim().toLocaleLowerCase();
    var haystack = aStr.split(/\s+/);
    var needles = aQuery.split(/\s+/);

    for (let needle of needles) {
      for (let hay of haystack) {
        if (hay == needle) {
          // matching whole words is best
          score += SEARCH_SCORE_MATCH_WHOLEWORD;
        } else {
          let i = hay.indexOf(needle);
          if (i == 0) // matching on word boundries is also good
            score += SEARCH_SCORE_MATCH_WORDBOUNDRY;
          else if (i > 0) // substring matches not so good
            score += SEARCH_SCORE_MATCH_SUBSTRING;
        }
      }
    }

    // give progressively higher score for longer queries, since longer queries
    // are more likely to be unique and therefore more relevant.
    if (needles.length > 1 && aStr.indexOf(aQuery) != -1)
      score += needles.length;

    return score * aMultiplier;
  },

  showEmptyNotice(aShow) {
    this._emptyNotice.hidden = !aShow;
    this._listBox.hidden = aShow;
  },

  showAllResultsLink(aTotalResults) {
    if (aTotalResults == 0) {
      this._allResultsLink.hidden = true;
      return;
    }

    var linkStr = gStrings.ext.GetStringFromName("showAllSearchResults");
    linkStr = PluralForm.get(aTotalResults, linkStr);
    linkStr = linkStr.replace("#1", aTotalResults);
    this._allResultsLink.setAttribute("value", linkStr);

    this._allResultsLink.setAttribute("href",
                                      AddonRepository.getSearchURL(this._lastQuery));
    this._allResultsLink.hidden = false;
 },

  updateListAttributes() {
    var item = this._listBox.querySelector("richlistitem[remote='true'][first]");
    if (item)
      item.removeAttribute("first");
    item = this._listBox.querySelector("richlistitem[remote='true'][last]");
    if (item)
      item.removeAttribute("last");
    var items = this._listBox.querySelectorAll("richlistitem[remote='true']");
    if (items.length > 0) {
      items[0].setAttribute("first", true);
      items[items.length - 1].setAttribute("last", true);
    }

    item = this._listBox.querySelector("richlistitem:not([remote='true'])[first]");
    if (item)
      item.removeAttribute("first");
    item = this._listBox.querySelector("richlistitem:not([remote='true'])[last]");
    if (item)
      item.removeAttribute("last");
    items = this._listBox.querySelectorAll("richlistitem:not([remote='true'])");
    if (items.length > 0) {
      items[0].setAttribute("first", true);
      items[items.length - 1].setAttribute("last", true);
    }

  },

  onSortChanged(aSortBy, aAscending) {
    var footer = this._listBox.lastChild;
    this._listBox.removeChild(footer);

    sortList(this._listBox, aSortBy, aAscending);
    this.updateListAttributes();

    this._listBox.appendChild(footer);
  },

  onDownloadCancelled(aInstall) {
    this.removeInstall(aInstall);
  },

  onInstallCancelled(aInstall) {
    this.removeInstall(aInstall);
  },

  onInstallEnded(aInstall) {
    // If this is a webextension that was installed from this page,
    // display the post-install notification.
    if (!WEBEXT_PERMISSION_PROMPTS || !aInstall.addon.isWebExtension) {
      return;
    }

    for (let item of this._listBox.childNodes) {
      if (item.mInstall == aInstall) {
        let subject = {
          wrappedJSObject: {
            target: getBrowserElement(),
            addon: aInstall.addon,
          },
        };
        Services.obs.notifyObservers(subject, "webextension-install-notify");
        return;
      }
    }
  },

  removeInstall(aInstall) {
    for (let item of this._listBox.childNodes) {
      if (item.mInstall == aInstall) {
        this._listBox.removeChild(item);
        return;
      }
    }
  },

  getSelectedAddon() {
    var item = this._listBox.selectedItem;
    if (item)
      return item.mAddon;
    return null;
  },

  getListItemForID(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.getAttribute("status") == "installed" && listitem.mAddon.id == aId)
        return listitem;
      listitem = listitem.nextSibling;
    }
    return null;
  }
};

var gLegacyView = {
  node: null,
  _listBox: null,
  _categoryItem: null,

  initialize() {
    this.node = document.getElementById("legacy-view");
    this._listBox = document.getElementById("legacy-list");
    this._categoryItem = gCategories.get("addons://legacy/");

    document.getElementById("legacy-learnmore").href = SUPPORT_URL + "webextensions";

    this.refresh();
  },

  async show(type, request) {
    let addons = await AddonManager.getAddonsByTypes(["extension"]);
    addons = addons.filter(a => !a.hidden &&
                              (isLegacyExtension(a) || isDisabledUnsigned(a)));

    while (this._listBox.itemCount > 0)
      this._listBox.removeItemAt(0);

    let elements = addons.map(a => createItem(a));
    sortElements(elements, ["uiState", "name"], true);
    for (let element of elements) {
      this._listBox.appendChild(element);
    }

    gViewController.notifyViewChanged();
  },

  hide() {},

  getSelectedAddon() {
    var item = this._listBox.selectedItem;
    if (item)
      return item.mAddon;
    return null;
  },

  async refresh() {
    if (legacyExtensionsEnabled) {
      this._categoryItem.disabled = true;
      return;
    }

    let extensions = await AddonManager.getAddonsByTypes(["extension"]);

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
      let name = gStrings.ext.GetStringFromName(`type.${haveUnsigned ? "unsupported" : "legacy"}.name`);
      this._categoryItem.setAttribute("name", name);
      this._categoryItem.tooltiptext = name;
    } else {
      this._categoryItem.disabled = true;
    }
  },
};

var gListView = {
  node: null,
  _listBox: null,
  _emptyNotice: null,
  _type: null,

  initialize() {
    this.node = document.getElementById("list-view");
    this._listBox = document.getElementById("addon-list");
    this._emptyNotice = document.getElementById("addon-list-empty");

    this._listBox.addEventListener("keydown", (aEvent) => {
      if (aEvent.keyCode == aEvent.DOM_VK_RETURN) {
        var item = this._listBox.selectedItem;
        if (item)
          item.showInDetailView();
      }
    });

    document.getElementById("signing-learn-more").setAttribute("href", SUPPORT_URL + "unsigned-addons");
    document.getElementById("legacy-extensions-learnmore-link").addEventListener("click", evt => {
      gViewController.loadView("addons://legacy/");
    });

    let findSignedAddonsLink = document.getElementById("find-alternative-addons");
    try {
      findSignedAddonsLink.setAttribute("href",
        Services.urlFormatter.formatURLPref("extensions.getAddons.link.url"));
    } catch (e) {
      findSignedAddonsLink.classList.remove("text-link");
    }

    try {
      document.getElementById("signing-dev-manual-link").setAttribute("href",
        Services.prefs.getCharPref("xpinstall.signatures.devInfoURL"));
    } catch (e) {
      document.getElementById("signing-dev-info").hidden = true;
    }

    if (Preferences.get("plugin.load_flash_only", true)) {
      document.getElementById("plugindeprecation-learnmore-link")
        .setAttribute("href", SUPPORT_URL + "npapi");
    } else {
      document.getElementById("plugindeprecation-notice").hidden = true;
    }

    if (Preferences.get("extensions.getAddons.themes.browseURL", "")) {
      document.getElementById("getthemes-learnmore-link")
        .setAttribute("href", Services.urlFormatter.formatURLPref("extensions.getAddons.themes.browseURL"));
    } else {
      document.getElementById("getthemes-container").hidden = true;
    }
  },

  show(aType, aRequest) {
    let showOnlyDisabledUnsigned = false;
    if (aType.endsWith("?unsigned=true")) {
      aType = aType.replace(/\?.*/, "");
      showOnlyDisabledUnsigned = true;
    }

    if (!(aType in AddonManager.addonTypes))
      throw Components.Exception("Attempting to show unknown type " + aType, Cr.NS_ERROR_INVALID_ARG);

    this._type = aType;
    this.node.setAttribute("type", aType);
    this.showEmptyNotice(false);

    while (this._listBox.itemCount > 0)
      this._listBox.removeItemAt(0);

    if (aType == "plugin") {
      navigator.plugins.refresh(false);
    }

    getAddonsAndInstalls(aType, (aAddonsList, aInstallsList) => {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      let showLegacyInfo = false;
      if (!legacyExtensionsEnabled) {
        let preLen = aAddonsList.length;
        aAddonsList = aAddonsList.filter(addon => !isLegacyExtension(addon) &&
                                                  !isDisabledUnsigned(addon));
        if (aAddonsList.length != preLen) {
          showLegacyInfo = true;
        }
      }

      var elements = [];

      for (let addonItem of aAddonsList)
        elements.push(createItem(addonItem));

      for (let installItem of aInstallsList)
        elements.push(createItem(installItem, true));

      this.showEmptyNotice(elements.length == 0);
      if (elements.length > 0) {
        sortElements(elements, ["uiState", "name"], true);
        for (let element of elements)
          this._listBox.appendChild(element);
      }

      this.filterDisabledUnsigned(showOnlyDisabledUnsigned);
      document.getElementById("legacy-extensions-notice").hidden = !showLegacyInfo;

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

    if (AddonSettings.REQUIRE_SIGNING) {
      for (let item of this._listBox.childNodes) {
        if (!isCorrectlySigned(item.mAddon))
          foundDisabledUnsigned = true;
        else
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

  onExternalInstall(aAddon, aExistingAddon, aRequiresRestart) {
    // The existing list item will take care of upgrade installs
    if (aExistingAddon)
      return;

    if (aAddon.hidden)
      return;

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
    if (aInstall.existingAddon)
      this.removeItem(aInstall, true);

    if (aInstall.addon.type == "experiment") {
      let item = this.getListItemForID(aInstall.addon.id);
      if (item) {
        item.endDate = getExperimentEndDate(aInstall.addon);
      }
    }
  },

  addItem(aObj, aIsInstall) {
    if (aObj.type != this._type)
      return;

    if (aIsInstall && aObj.existingAddon)
      return;

    let prop = aIsInstall ? "mInstall" : "mAddon";
    for (let item of this._listBox.childNodes) {
      if (item[prop] == aObj)
        return;
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
    if (item)
      return item.mAddon;
    return null;
  },

  getListItemForID(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.getAttribute("status") == "installed" && listitem.mAddon.id == aId)
        return listitem;
      listitem = listitem.nextSibling;
    }
    return null;
  }
};


var gDetailView = {
  node: null,
  _addon: null,
  _loadingTimer: null,
  _autoUpdate: null,

  initialize() {
    this.node = document.getElementById("detail-view");

    this._autoUpdate = document.getElementById("detail-autoUpdate");

    this._autoUpdate.addEventListener("command", () => {
      this._addon.applyBackgroundUpdates = this._autoUpdate.value;
    }, true);
  },

  shutdown() {
    AddonManager.removeManagerListener(this);
  },

  onUpdateModeChanged() {
    this.onPropertyChanged(["applyBackgroundUpdates"]);
  },

  _updateView(aAddon, aIsRemote, aScrollToPreferences) {
    AddonManager.addManagerListener(this);
    this.clearLoading();

    this._addon = aAddon;
    gEventManager.registerAddonListener(this, aAddon.id);
    gEventManager.registerInstallListener(this);

    this.node.setAttribute("type", aAddon.type);

    let legacy = false;
    if (!aAddon.install) {
      if (aAddon.type == "extension" && !aAddon.isWebExtension) {
        legacy = true;
      }
      if (aAddon.type == "theme") {
        // The logic here is kind of clunky but we want to mark complete
        // themes as legacy.  There's no explicit flag for complete
        // themes so explicitly check for new style themes (for which
        // isWebExtension is true) or lightweight themes (which have
        // ids that end with @personas.mozilla.org)
        legacy = !(aAddon.isWebExtension || aAddon.id.endsWith("@personas.mozilla.org"));
      }

      if (legacy && aAddon.signedStatus == AddonManager.SIGNEDSTATE_PRIVILEGED) {
        legacy = false;
      }

      // Exceptions that can slip through above: the default theme plus
      // test pilot addons until we get SIGNEDSTATE_PRIVILEGED deployed.
      if (legacy && legacyWarningExceptions.includes(aAddon.id)) {
        legacy = false;
      }
    }
    this.node.setAttribute("legacy", legacy);
    document.getElementById("detail-legacy-warning").href = SUPPORT_URL + "webextensions";

    // If the search category isn't selected then make sure to select the
    // correct category
    if (gCategories.selected != "addons://search/")
      gCategories.select("addons://list/" + aAddon.type);

    document.getElementById("detail-name").textContent = aAddon.name;
    var icon = AddonManager.getPreferredIconURL(aAddon, 64, window);
    document.getElementById("detail-icon").src = icon ? icon : "";
    document.getElementById("detail-creator").setCreator(aAddon.creator, aAddon.homepageURL);

    var version = document.getElementById("detail-version");
    if (shouldShowVersionNumber(aAddon)) {
      version.hidden = false;
      version.value = aAddon.version;
    } else {
      version.hidden = true;
    }

    var screenshotbox = document.getElementById("detail-screenshot-box");
    var screenshot = document.getElementById("detail-screenshot");
    if (aAddon.screenshots && aAddon.screenshots.length > 0) {
      if (aAddon.screenshots[0].thumbnailURL) {
        screenshot.src = aAddon.screenshots[0].thumbnailURL;
        screenshot.width = aAddon.screenshots[0].thumbnailWidth;
        screenshot.height = aAddon.screenshots[0].thumbnailHeight;
      } else {
        screenshot.src = aAddon.screenshots[0].url;
        screenshot.width = aAddon.screenshots[0].width;
        screenshot.height = aAddon.screenshots[0].height;
      }
      screenshot.setAttribute("loading", "true");
      screenshotbox.hidden = false;
    } else {
      screenshotbox.hidden = true;
    }

    var desc = document.getElementById("detail-desc");
    desc.textContent = aAddon.description;

    var fullDesc = document.getElementById("detail-fulldesc");
    if (aAddon.fullDescription) {
      // The following is part of an awful hack to include the licenses for GMP
      // plugins without having bug 624602 fixed yet, and intentionally ignores
      // localisation.
      if (aAddon.isGMPlugin) {
        fullDesc.innerHTML = aAddon.fullDescription;
      } else {
        fullDesc.textContent = aAddon.fullDescription;
      }

      fullDesc.hidden = false;
    } else {
      fullDesc.hidden = true;
    }

    var contributions = document.getElementById("detail-contributions");
    if ("contributionURL" in aAddon && aAddon.contributionURL) {
      contributions.hidden = false;
      var amount = document.getElementById("detail-contrib-suggested");
      if (aAddon.contributionAmount) {
        amount.value = gStrings.ext.formatStringFromName("contributionAmount2",
                                                         [aAddon.contributionAmount],
                                                         1);
        amount.hidden = false;
      } else {
        amount.hidden = true;
      }
    } else {
      contributions.hidden = true;
    }

    if ("purchaseURL" in aAddon && aAddon.purchaseURL) {
      var purchase = document.getElementById("detail-purchase-btn");
      purchase.label = gStrings.ext.formatStringFromName("cmd.purchaseAddon.label",
                                                         [aAddon.purchaseDisplayAmount],
                                                         1);
      purchase.accesskey = gStrings.ext.GetStringFromName("cmd.purchaseAddon.accesskey");
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
      text = PluralForm.get(aAddon.reviewCount, text)
      text = text.replace("#1", aAddon.reviewCount);
      reviews.value = text;
      reviews.hidden = false;
      reviews.href = aAddon.reviewURL;
    } else {
      reviews.hidden = true;
    }

    document.getElementById("detail-rating-row").hidden = !aAddon.averageRating && !aAddon.reviewURL;

    var sizeRow = document.getElementById("detail-size");
    if (aAddon.size && aIsRemote) {
      let [size, unit] = DownloadUtils.convertByteUnits(parseInt(aAddon.size));
      let formatted = gStrings.dl.GetStringFromName("doneSize");
      formatted = formatted.replace("#1", size).replace("#2", unit);
      sizeRow.value = formatted;
    } else {
      sizeRow.value = null;
    }

    var downloadsRow = document.getElementById("detail-downloads");
    if (aAddon.totalDownloads && aIsRemote) {
      var downloads = aAddon.totalDownloads;
      downloadsRow.value = downloads;
    } else {
      downloadsRow.value = null;
    }

    var canUpdate = !aIsRemote && hasPermission(aAddon, "upgrade") && aAddon.id != AddonManager.hotfixID;
    document.getElementById("detail-updates-row").hidden = !canUpdate;

    if ("applyBackgroundUpdates" in aAddon) {
      this._autoUpdate.hidden = false;
      this._autoUpdate.value = aAddon.applyBackgroundUpdates;
      let hideFindUpdates = AddonManager.shouldAutoUpdate(this._addon);
      document.getElementById("detail-findUpdates-btn").hidden = hideFindUpdates;
    } else {
      this._autoUpdate.hidden = true;
      document.getElementById("detail-findUpdates-btn").hidden = false;
    }

    document.getElementById("detail-prefs-btn").hidden = !aIsRemote &&
      !gViewController.commands.cmd_showItemPreferences.isEnabled(aAddon);

    var gridRows = document.querySelectorAll("#detail-grid rows row");
    let first = true;
    for (let gridRow of gridRows) {
      if (first && window.getComputedStyle(gridRow).getPropertyValue("display") != "none") {
        gridRow.setAttribute("first-row", true);
        first = false;
      } else {
        gridRow.removeAttribute("first-row");
      }
    }

    if (this._addon.type == "experiment") {
      let prefix = "details.experiment.";
      let active = this._addon.isActive;

      let stateKey = prefix + "state." + (active ? "active" : "complete");
      let node = document.getElementById("detail-experiment-state");
      node.value = gStrings.ext.GetStringFromName(stateKey);

      let now = Date.now();
      let end = getExperimentEndDate(this._addon);
      let days = Math.abs(end - now) / (24 * 60 * 60 * 1000);

      let timeKey = prefix + "time.";
      let timeMessage;
      if (days < 1) {
        timeKey += (active ? "endsToday" : "endedToday");
        timeMessage = gStrings.ext.GetStringFromName(timeKey);
      } else {
        timeKey += (active ? "daysRemaining" : "daysPassed");
        days = Math.round(days);
        let timeString = gStrings.ext.GetStringFromName(timeKey);
        timeMessage = PluralForm.get(days, timeString)
                                .replace("#1", days);
      }

      document.getElementById("detail-experiment-time").value = timeMessage;
    }

    this.fillSettingsRows(aScrollToPreferences, () => {
      this.updateState();
      gViewController.notifyViewChanged();
    });
  },

  show(aAddonId, aRequest) {
    let index = aAddonId.indexOf("/preferences");
    let scrollToPreferences = false;
    if (index >= 0) {
      aAddonId = aAddonId.substring(0, index);
      scrollToPreferences = true;
    }

    this._loadingTimer = setTimeout(() => {
      this.node.setAttribute("loading-extended", true);
    }, LOADING_MSG_DELAY);

    AddonManager.getAddonByID(aAddonId, (aAddon) => {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      if (aAddon) {
        this._updateView(aAddon, false, scrollToPreferences);
        return;
      }

      // Look for an add-on pending install
      AddonManager.getAllInstalls(aInstalls => {
        for (let install of aInstalls) {
          if (install.state == AddonManager.STATE_INSTALLED &&
              install.addon.id == aAddonId) {
            this._updateView(install.addon, false);
            return;
          }
        }

        if (aAddonId in gCachedAddons) {
          this._updateView(gCachedAddons[aAddonId], true);
          return;
        }

        // This might happen due to session restore restoring us back to an
        // add-on that doesn't exist but otherwise shouldn't normally happen.
        // Either way just revert to the default view.
        gViewController.replaceView(gViewDefault);
      });
    });
  },

  hide() {
    AddonManager.removeManagerListener(this);
    this.clearLoading();
    if (this._addon) {
      if (hasInlineOptions(this._addon)) {
        Services.obs.notifyObservers(document,
                                     AddonManager.OPTIONS_NOTIFICATION_HIDDEN,
                                     this._addon.id);
      }

      gEventManager.unregisterAddonListener(this, this._addon.id);
      gEventManager.unregisterInstallListener(this);
      this._addon = null;

      // Flush the preferences to disk so they survive any crash
      if (this.node.getElementsByTagName("setting").length)
        Services.prefs.savePrefFile(null);
    }
  },

  updateState() {
    gViewController.updateCommands();

    var pending = this._addon.pendingOperations;
    if (pending != AddonManager.PENDING_NONE) {
      this.node.removeAttribute("notification");

      pending = null;
      const PENDING_OPERATIONS = ["enable", "disable", "install", "uninstall",
                                  "upgrade"];
      for (let op of PENDING_OPERATIONS) {
        if (isPending(this._addon, op))
          pending = op;
      }

      this.node.setAttribute("pending", pending);
      document.getElementById("detail-pending").textContent = gStrings.ext.formatStringFromName(
        "details.notification." + pending,
        [this._addon.name, gStrings.brandShortName], 2
      );
    } else {
      this.node.removeAttribute("pending");

      if (this._addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED) {
        this.node.setAttribute("notification", "error");
        document.getElementById("detail-error").textContent = gStrings.ext.formatStringFromName(
          "details.notification.blocked",
          [this._addon.name], 1
        );
        var errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName("details.notification.blocked.link");
        errorLink.href = this._addon.blocklistURL;
        errorLink.hidden = false;
      } else if (isDisabledUnsigned(this._addon)) {
        this.node.setAttribute("notification", "error");
        document.getElementById("detail-error").textContent = gStrings.ext.formatStringFromName(
          "details.notification.unsignedAndDisabled", [this._addon.name, gStrings.brandShortName], 2
        );
        let errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName("details.notification.unsigned.link");
        errorLink.href = SUPPORT_URL + "unsigned-addons";
        errorLink.hidden = false;
      } else if (!this._addon.isCompatible && (AddonManager.checkCompatibility ||
        (this._addon.blocklistState != Ci.nsIBlocklistService.STATE_SOFTBLOCKED))) {
        this.node.setAttribute("notification", "warning");
        document.getElementById("detail-warning").textContent = gStrings.ext.formatStringFromName(
          "details.notification.incompatible",
          [this._addon.name, gStrings.brandShortName, gStrings.appVersion], 3
        );
        document.getElementById("detail-warning-link").hidden = true;
      } else if (this._addon.appDisabled && !this._addon.multiprocessCompatible && !ALLOW_NON_MPC) {
        this.node.setAttribute("notification", "error");
        document.getElementById("detail-error").textContent = gStrings.ext.formatStringFromName(
          "details.notification.nonMpcDisabled", [this._addon.name], 1
        );
        let errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName("details.notification.nonMpcDisabled.link");
        errorLink.href = "https://wiki.mozilla.org/Add-ons/ShimsNightly";
      } else if (!isCorrectlySigned(this._addon)) {
        this.node.setAttribute("notification", "warning");
        document.getElementById("detail-warning").textContent = gStrings.ext.formatStringFromName(
          "details.notification.unsigned", [this._addon.name, gStrings.brandShortName], 2
        );
        var warningLink = document.getElementById("detail-warning-link");
        warningLink.value = gStrings.ext.GetStringFromName("details.notification.unsigned.link");
        warningLink.href = SUPPORT_URL + "unsigned-addons";
        warningLink.hidden = false;
      } else if (this._addon.blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED) {
        this.node.setAttribute("notification", "warning");
        document.getElementById("detail-warning").textContent = gStrings.ext.formatStringFromName(
          "details.notification.softblocked",
          [this._addon.name], 1
        );
        let warningLink = document.getElementById("detail-warning-link");
        warningLink.value = gStrings.ext.GetStringFromName("details.notification.softblocked.link");
        warningLink.href = this._addon.blocklistURL;
        warningLink.hidden = false;
      } else if (this._addon.blocklistState == Ci.nsIBlocklistService.STATE_OUTDATED) {
        this.node.setAttribute("notification", "warning");
        document.getElementById("detail-warning").textContent = gStrings.ext.formatStringFromName(
          "details.notification.outdated",
          [this._addon.name], 1
        );
        let warningLink = document.getElementById("detail-warning-link");
        warningLink.value = gStrings.ext.GetStringFromName("details.notification.outdated.link");
        warningLink.href = this._addon.blocklistURL;
        warningLink.hidden = false;
      } else if (this._addon.blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE) {
        this.node.setAttribute("notification", "error");
        document.getElementById("detail-error").textContent = gStrings.ext.formatStringFromName(
          "details.notification.vulnerableUpdatable",
          [this._addon.name], 1
        );
        let errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName("details.notification.vulnerableUpdatable.link");
        errorLink.href = this._addon.blocklistURL;
        errorLink.hidden = false;
      } else if (this._addon.blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE) {
        this.node.setAttribute("notification", "error");
        document.getElementById("detail-error").textContent = gStrings.ext.formatStringFromName(
          "details.notification.vulnerableNoUpdate",
          [this._addon.name], 1
        );
        let errorLink = document.getElementById("detail-error-link");
        errorLink.value = gStrings.ext.GetStringFromName("details.notification.vulnerableNoUpdate.link");
        errorLink.href = this._addon.blocklistURL;
        errorLink.hidden = false;
      } else if (this._addon.isGMPlugin && !this._addon.isInstalled &&
                 this._addon.isActive) {
        this.node.setAttribute("notification", "warning");
        let warning = document.getElementById("detail-warning");
        warning.textContent =
          gStrings.ext.formatStringFromName("details.notification.gmpPending",
                                            [this._addon.name], 1);
      } else {
        this.node.removeAttribute("notification");
      }
    }

    let menulist = document.getElementById("detail-state-menulist");
    let addonType = AddonManager.addonTypes[this._addon.type];
    if (addonType.flags & AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE) {
      let askItem = document.getElementById("detail-ask-to-activate-menuitem");
      let alwaysItem = document.getElementById("detail-always-activate-menuitem");
      let neverItem = document.getElementById("detail-never-activate-menuitem");
      let hasActivatePermission =
        ["ask_to_activate", "enable", "disable"].some(perm => hasPermission(this._addon, perm));

      if (!this._addon.isActive) {
        menulist.selectedItem = neverItem;
      } else if (this._addon.userDisabled == AddonManager.STATE_ASK_TO_ACTIVATE) {
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
    var lastRow = document.getElementById("detail-downloads");
    var rows = lastRow.parentNode;
    while (lastRow.nextSibling)
      rows.removeChild(rows.lastChild);
  },

  fillSettingsRows(aScrollToPreferences, aCallback) {
    this.emptySettingsRows();
    if (!hasInlineOptions(this._addon)) {
      if (aCallback)
        aCallback();
      return;
    }

    // We can't use a promise for this, since some code (especially in tests)
    // relies on us finishing before the ViewChanged event bubbles up to its
    // listeners, and promises resolve asynchronously.
    let whenViewLoaded = callback => {
      if (gViewController.displayedView.hasAttribute("loading")) {
        gDetailView.node.addEventListener("ViewChanged", function() {
          callback();
        }, {once: true});
      } else {
        callback();
      }
    };

    let finish = (firstSetting) => {
      // Ensure the page has loaded and force the XBL bindings to be synchronously applied,
      // then notify observers.
      whenViewLoaded(() => {
        if (firstSetting)
          firstSetting.clientTop;
        Services.obs.notifyObservers(document,
                                     AddonManager.OPTIONS_NOTIFICATION_DISPLAYED,
                                     this._addon.id);
        if (aScrollToPreferences)
          gDetailView.scrollToPreferencesRows();
      });
    }

    // This function removes and returns the text content of aNode without
    // removing any child elements. Removing the text nodes ensures any XBL
    // bindings apply properly.
    function stripTextNodes(aNode) {
      var text = "";
      for (var i = 0; i < aNode.childNodes.length; i++) {
        if (aNode.childNodes[i].nodeType != document.ELEMENT_NODE) {
          text += aNode.childNodes[i].textContent;
          aNode.removeChild(aNode.childNodes[i--]);
        } else {
          text += stripTextNodes(aNode.childNodes[i]);
        }
      }
      return text;
    }

    var rows = document.getElementById("detail-downloads").parentNode;

    try {
      if (this._addon.optionsType == AddonManager.OPTIONS_TYPE_INLINE_BROWSER) {
        whenViewLoaded(() => {
          this.createOptionsBrowser(rows).then(browser => {
            // Make sure the browser is unloaded as soon as we change views,
            // rather than waiting for the next detail view to load.
            document.addEventListener("ViewChanged", function() {
              browser.remove();
            }, {once: true});

            finish(browser);
          });
        });

        if (aCallback)
          aCallback();
      } else {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", this._addon.optionsURL, true);
        xhr.responseType = "xml";
        xhr.onload = (function() {
          var xml = xhr.responseXML;
          var settings = xml.querySelectorAll(":root > setting");

          var firstSetting = null;
          for (var setting of settings) {

            var desc = stripTextNodes(setting).trim();
            if (!setting.hasAttribute("desc"))
              setting.setAttribute("desc", desc);

            var type = setting.getAttribute("type");
            if (type == "file" || type == "directory")
              setting.setAttribute("fullpath", "true");

            setting = document.importNode(setting, true);
            var style = setting.getAttribute("style");
            if (style) {
              setting.removeAttribute("style");
              setting.setAttribute("style", style);
            }

            rows.appendChild(setting);
            var visible = window.getComputedStyle(setting).getPropertyValue("display") != "none";
            if (!firstSetting && visible) {
              setting.setAttribute("first-row", true);
              firstSetting = setting;
            }
          }

          finish(firstSetting);

          if (aCallback)
            aCallback();
        });
        xhr.onerror = function(aEvent) {
          Cu.reportError("Error " + aEvent.target.status +
                         " occurred while receiving " + this._addon.optionsURL);
          if (aCallback)
            aCallback();
        };
        xhr.send();
      }
    } catch (e) {
      Cu.reportError(e);
      if (aCallback)
        aCallback();
    }
  },

  scrollToPreferencesRows() {
    // We find this row, rather than remembering it from above,
    // in case it has been changed by the observers.
    let firstRow = gDetailView.node.querySelector('setting[first-row="true"]');
    if (firstRow) {
      let top = firstRow.boxObject.y;
      top -= parseInt(window.getComputedStyle(firstRow).getPropertyValue("margin-top"));

      let detailViewBoxObject = gDetailView.node.boxObject;
      top -= detailViewBoxObject.y;

      detailViewBoxObject.scrollTo(0, top);
    }
  },

  async createOptionsBrowser(parentNode) {
    let browser = document.createElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("class", "inline-options-browser");

    let {optionsURL} = this._addon;
    let remote = !E10SUtils.canLoadURIInProcess(optionsURL, Services.appinfo.PROCESS_TYPE_DEFAULT);

    let readyPromise;
    if (remote) {
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", E10SUtils.EXTENSION_REMOTE_TYPE);
      readyPromise = promiseEvent("XULFrameLoaderCreated", browser);
    } else {
      readyPromise = promiseEvent("load", browser, true);
    }

    parentNode.appendChild(browser);

    // Force bindings to apply synchronously.
    browser.clientTop;

    await readyPromise;
    if (remote) {
      ExtensionParent.apiManager.emit("extension-browser-inserted", browser);
    }

    return new Promise(resolve => {
      let messageListener = {
        receiveMessage({name, data}) {
          if (name === "Extension:BrowserResized")
            browser.style.height = `${data.height}px`;
          else if (name === "Extension:BrowserContentLoaded")
            resolve(browser);
        },
      };

      let mm = browser.messageManager || new FakeFrameMessageManager(browser);
      mm.loadFrameScript("chrome://extensions/content/ext-browser-content.js",
                         false);
      mm.addMessageListener("Extension:BrowserContentLoaded", messageListener);
      mm.addMessageListener("Extension:BrowserResized", messageListener);

      let browserOptions = {
        fixedWidth: true,
        isInline: true,
      };

      if (this._addon.optionsBrowserStyle) {
        browserOptions.stylesheets = extensionStylesheets;
      }

      mm.sendAsyncMessage("Extension:InitBrowser", browserOptions);

      browser.loadURI(optionsURL);
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

  onDisabling(aNeedsRestart) {
    this.updateState();
    if (!aNeedsRestart && hasInlineOptions(this._addon)) {
      Services.obs.notifyObservers(document,
                                   AddonManager.OPTIONS_NOTIFICATION_HIDDEN,
                                   this._addon.id);
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
    if (aProperties.indexOf("applyBackgroundUpdates") != -1) {
      this._autoUpdate.value = this._addon.applyBackgroundUpdates;
      let hideFindUpdates = AddonManager.shouldAutoUpdate(this._addon);
      document.getElementById("detail-findUpdates-btn").hidden = hideFindUpdates;
    }

    if (aProperties.indexOf("appDisabled") != -1 ||
        aProperties.indexOf("signedState") != -1 ||
        aProperties.indexOf("userDisabled") != -1)
      this.updateState();
  },

  onExternalInstall(aAddon, aExistingAddon, aNeedsRestart) {
    // Only care about upgrades for the currently displayed add-on
    if (!aExistingAddon || aExistingAddon.id != this._addon.id)
      return;

    if (!aNeedsRestart)
      this._updateView(aAddon, false);
    else
      this.updateState();
  },

  onInstallCancelled(aInstall) {
    if (aInstall.addon.id == this._addon.id)
      gViewController.popState();
  }
};


var gUpdatesView = {
  node: null,
  _listBox: null,
  _emptyNotice: null,
  _sorters: null,
  _updateSelected: null,
  _categoryItem: null,

  initialize() {
    this.node = document.getElementById("updates-view");
    this._listBox = document.getElementById("updates-list");
    this._emptyNotice = document.getElementById("updates-list-empty");
    this._sorters = document.getElementById("updates-sorters");
    this._sorters.handler = this;

    this._categoryItem = gCategories.get("addons://updates/available");

    this._updateSelected = document.getElementById("update-selected-btn");
    this._updateSelected.addEventListener("command", function() {
      gUpdatesView.installSelected();
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
    document.getElementById("empty-availableUpdates-msg").hidden = aType != "available";
    document.getElementById("empty-recentUpdates-msg").hidden = aType != "recent";
    this.showEmptyNotice(false);

    while (this._listBox.itemCount > 0)
      this._listBox.removeItemAt(0);

    this.node.setAttribute("updatetype", aType);
    if (aType == "recent")
      this._showRecentUpdates(aRequest);
    else
      this._showAvailableUpdates(false, aRequest);
  },

  hide() {
    this._updateSelected.hidden = true;
    this._categoryItem.disabled = this._categoryItem.badgeCount == 0;
    doPendingUninstalls(this._listBox);
  },

  _showRecentUpdates(aRequest) {
    AddonManager.getAllAddons((aAddonsList) => {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      var elements = [];
      let threshold = Date.now() - UPDATES_RECENT_TIMESPAN;
      for (let addon of aAddonsList) {
        if (addon.hidden || !addon.updateDate || addon.updateDate.getTime() < threshold)
          continue;

        elements.push(createItem(addon));
      }

      this.showEmptyNotice(elements.length == 0);
      if (elements.length > 0) {
        sortElements(elements, [this._sorters.sortBy], this._sorters.ascending);
        for (let element of elements)
          this._listBox.appendChild(element);
      }

      gViewController.notifyViewChanged();
    });
  },

  _showAvailableUpdates(aIsRefresh, aRequest) {
    /* Disable the Update Selected button so it can't get clicked
       before everything is initialized asynchronously.
       It will get re-enabled by maybeDisableUpdateSelected(). */
    this._updateSelected.disabled = true;

    AddonManager.getAllInstalls((aInstallsList) => {
      if (!aIsRefresh && gViewController && aRequest &&
          aRequest != gViewController.currentViewRequest)
        return;

      if (aIsRefresh) {
        this.showEmptyNotice(false);
        this._updateSelected.hidden = true;

        while (this._listBox.childNodes.length > 0)
          this._listBox.firstChild.remove();
      }

      var elements = [];

      for (let install of aInstallsList) {
        if (!this.isManualUpdate(install))
          continue;

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
        sortElements(elements, [this._sorters.sortBy], this._sorters.ascending);
        for (let element of elements)
          this._listBox.appendChild(element);
      }

      // ensure badge count is in sync
      this._categoryItem.badgeCount = this._listBox.itemCount;

      gViewController.notifyViewChanged();
    });
  },

  showEmptyNotice(aShow) {
    this._emptyNotice.hidden = !aShow;
    this._listBox.hidden = aShow;
  },

  isManualUpdate(aInstall, aOnlyAvailable) {
    var isManual = aInstall.existingAddon &&
                   !AddonManager.shouldAutoUpdate(aInstall.existingAddon);
    if (isManual && aOnlyAvailable)
      return isInState(aInstall, "available");
    return isManual;
  },

  maybeRefresh() {
    if (gViewController.currentViewId == "addons://updates/available")
      this._showAvailableUpdates(true);
    this.updateAvailableCount();
  },

  updateAvailableCount(aInitializing) {
    if (aInitializing)
      gPendingInitializations++;
    AddonManager.getAllInstalls((aInstallsList) => {
      var count = aInstallsList.filter(aInstall => {
        return this.isManualUpdate(aInstall, true);
      }).length;
      this._categoryItem.disabled = gViewController.currentViewId != "addons://updates/available" &&
                                    count == 0;
      this._categoryItem.badgeCount = count;
      if (aInitializing)
        notifyInitialized();
    });
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
      if (item.includeUpdate)
        item.upgrade();
    }

    this._updateSelected.disabled = true;
  },

  getSelectedAddon() {
    var item = this._listBox.selectedItem;
    if (item)
      return item.mAddon;
    return null;
  },

  getListItemForID(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.mAddon.id == aId)
        return listitem;
      listitem = listitem.nextSibling;
    }
    return null;
  },

  onSortChanged(aSortBy, aAscending) {
    sortList(this._listBox, aSortBy, aAscending);
  },

  onNewInstall(aInstall) {
    if (!this.isManualUpdate(aInstall))
      return;
    this.maybeRefresh();
  },

  onInstallStarted(aInstall) {
    this.updateAvailableCount();
  },

  onInstallCancelled(aInstall) {
    if (!this.isManualUpdate(aInstall))
      return;
    this.maybeRefresh();
  },

  onPropertyChanged(aAddon, aProperties) {
    if (aProperties.indexOf("applyBackgroundUpdates") != -1)
      this.updateAvailableCount();
  }
};

var gDragDrop = {
  onDragOver(aEvent) {
    var types = aEvent.dataTransfer.types;
    if (types.includes("text/uri-list") ||
        types.includes("text/x-moz-url") ||
        types.includes("application/x-moz-file"))
      aEvent.preventDefault();
  },

  onDrop(aEvent) {
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
        AddonManager.getInstallForURL(url, install => {
          AddonManager.installAddonFromAOM(browser, document.documentURIObject, install);
        }, "application/x-xpinstall");
      }
    }

    aEvent.preventDefault();
  }
};
