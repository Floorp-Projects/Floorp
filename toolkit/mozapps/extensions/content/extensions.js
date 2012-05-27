/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;


Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource://gre/modules/DownloadUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/AddonRepository.jsm");


const PREF_DISCOVERURL = "extensions.webservice.discoverURL";
const PREF_MAXRESULTS = "extensions.getAddons.maxResults";
const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_GETADDONS_CACHE_ID_ENABLED = "extensions.%ID%.getAddons.cache.enabled";
const PREF_UI_TYPE_HIDDEN = "extensions.ui.%TYPE%.hidden";
const PREF_UI_LASTCATEGORY = "extensions.ui.lastCategory";

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

const VIEW_DEFAULT = "addons://discover/";

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

document.addEventListener("load", initialize, true);
window.addEventListener("unload", shutdown, false);

var gPendingInitializations = 1;
__defineGetter__("gIsInitializing", function() gPendingInitializations > 0);

function initialize(event) {
  // XXXbz this listener gets _all_ load events for all nodes in the
  // document... but relies on not being called "too early".
  if (event.target instanceof XMLStylesheetProcessingInstruction) {
    return;
  }
  document.removeEventListener("load", initialize, true);
  gViewController.initialize();
  gCategories.initialize();
  gHeader.initialize();
  gEventManager.initialize();
  Services.obs.addObserver(sendEMPong, "EM-ping", false);
  Services.obs.notifyObservers(window, "EM-loaded", "");

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
      "view" in window.arguments[0]) {
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
  Services.obs.notifyObservers(window, "EM-pong", "");
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

  back: function() {
    window.history.back();
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  forward: function() {
    window.history.forward();
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  pushState: function(aState) {
    window.history.pushState(aState, document.title);
  },

  replaceState: function(aState) {
    window.history.replaceState(aState, document.title);
  },

  popState: function() {
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

  back: function() {
    if (this.pos == 0)
      throw new Error("Cannot go back from this point");

    this.pos--;
    gViewController.updateState(this.states[this.pos]);
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  forward: function() {
    if ((this.pos + 1) >= this.states.length)
      throw new Error("Cannot go forward from this point");

    this.pos++;
    gViewController.updateState(this.states[this.pos]);
    gViewController.updateCommand("cmd_back");
    gViewController.updateCommand("cmd_forward");
  },

  pushState: function(aState) {
    this.pos++;
    this.states.splice(this.pos, this.states.length);
    this.states.push(aState);
  },

  replaceState: function(aState) {
    this.states[this.pos] = aState;
  },

  popState: function() {
    if (this.pos == 0)
      throw new Error("Cannot popState from this view");

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
}
else {
  gHistory = FakeHistory;
}

var gEventManager = {
  _listeners: {},
  _installListeners: [],

  initialize: function() {
    var self = this;
    ["onEnabling", "onEnabled", "onDisabling", "onDisabled", "onUninstalling",
     "onUninstalled", "onInstalled", "onOperationCancelled",
     "onUpdateAvailable", "onUpdateFinished", "onCompatibilityUpdateAvailable",
     "onPropertyChanged"].forEach(function(aEvent) {
      self[aEvent] = function() {
        self.delegateAddonEvent(aEvent, Array.splice(arguments, 0));
      };
    });

    ["onNewInstall", "onDownloadStarted", "onDownloadEnded", "onDownloadFailed",
     "onDownloadProgress", "onDownloadCancelled", "onInstallStarted",
     "onInstallEnded", "onInstallFailed", "onInstallCancelled",
     "onExternalInstall"].forEach(function(aEvent) {
      self[aEvent] = function() {
        self.delegateInstallEvent(aEvent, Array.splice(arguments, 0));
      };
    });

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
      var countEnabledMenuCmds = 0;
      for (let child of contextMenu.children) {
        if (child.nodeName == "menuitem" &&
          gViewController.isCommandEnabled(child.command)) {
            countEnabledMenuCmds++;
        }
      }
      
      // with only one menu item, we hide the menu separator
      menuSep.hidden = (countEnabledMenuCmds <= 1);
      
    }, false);
  },

  shutdown: function() {
    AddonManager.removeManagerListener(this);
    AddonManager.removeInstallListener(this);
    AddonManager.removeAddonListener(this);
  },

  registerAddonListener: function(aListener, aAddonId) {
    if (!(aAddonId in this._listeners))
      this._listeners[aAddonId] = [];
    else if (this._listeners[aAddonId].indexOf(aListener) != -1)
      return;
    this._listeners[aAddonId].push(aListener);
  },

  unregisterAddonListener: function(aListener, aAddonId) {
    if (!(aAddonId in this._listeners))
      return;
    var index = this._listeners[aAddonId].indexOf(aListener);
    if (index == -1)
      return;
    this._listeners[aAddonId].splice(index, 1);
  },

  registerInstallListener: function(aListener) {
    if (this._installListeners.indexOf(aListener) != -1)
      return;
    this._installListeners.push(aListener);
  },

  unregisterInstallListener: function(aListener) {
    var i = this._installListeners.indexOf(aListener);
    if (i == -1)
      return;
    this._installListeners.splice(i, 1);
  },

  delegateAddonEvent: function(aEvent, aParams) {
    var addon = aParams.shift();
    if (!(addon.id in this._listeners))
      return;

    var listeners = this._listeners[addon.id];
    for (let listener of listeners) {
      if (!(aEvent in listener))
        continue;
      try {
        listener[aEvent].apply(listener, aParams);
      } catch(e) {
        // this shouldn't be fatal
        Cu.reportError(e);
      }
    }
  },

  delegateInstallEvent: function(aEvent, aParams) {
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
      } catch(e) {
        // this shouldn't be fatal
        Cu.reportError(e);
      }
    }
  },
  
  refreshGlobalWarning: function() {
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

  refreshAutoUpdateDefault: function() {
    var updateEnabled = AddonManager.updateEnabled;
    var autoUpdateDefault = AddonManager.autoUpdateDefault;

    // The checkbox needs to reflect that both prefs need to be true
    // for updates to be checked for and applied automatically
    document.getElementById("utils-autoUpdateDefault")
            .setAttribute("checked", updateEnabled && autoUpdateDefault);

    document.getElementById("utils-resetAddonUpdatesToAutomatic").hidden = !autoUpdateDefault;
    document.getElementById("utils-resetAddonUpdatesToManual").hidden = autoUpdateDefault;
  },

  onCompatibilityModeChanged: function() {
    this.refreshGlobalWarning();
  },

  onCheckUpdateSecurityChanged: function() {
    this.refreshGlobalWarning();
  },

  onUpdateModeChanged: function() {
    this.refreshAutoUpdateDefault();
  }
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

  initialize: function() {
    this.viewPort = document.getElementById("view-port");

    this.viewObjects["search"] = gSearchView;
    this.viewObjects["discover"] = gDiscoverView;
    this.viewObjects["list"] = gListView;
    this.viewObjects["detail"] = gDetailView;
    this.viewObjects["updates"] = gUpdatesView;

    for each (let view in this.viewObjects)
      view.initialize();

    window.controllers.appendController(this);

    window.addEventListener("popstate",
                            function (e) {
                              gViewController.updateState(e.state);
                            },
                            false);
  },

  shutdown: function() {
    if (this.currentViewObj)
      this.currentViewObj.hide();
    this.currentViewRequest = 0;

    for each(let view in this.viewObjects) {
      if ("shutdown" in view) {
        try {
          view.shutdown();
        } catch(e) {
          // this shouldn't be fatal
          Cu.reportError(e);
        }
      }
    }

    window.controllers.removeController(this);
  },

  updateState: function(state) {
    try {
      this.loadViewInternal(state.view, state.previousView, state);
      this.lastHistoryIndex = gHistory.index;
    }
    catch (e) {
      // The attempt to load the view failed, try moving further along history
      if (this.lastHistoryIndex > gHistory.index) {
        if (gHistory.canGoBack)
          gHistory.back();
        else
          gViewController.replaceView(VIEW_DEFAULT);
      } else {
        if (gHistory.canGoForward)
          gHistory.forward();
        else
          gViewController.replaceView(VIEW_DEFAULT);
      }
    }
  },

  parseViewId: function(aViewId) {
    var matchRegex = /^addons:\/\/([^\/]+)\/(.*)$/;
    var [,viewType, viewParam] = aViewId.match(matchRegex) || [];
    return {type: viewType, param: decodeURIComponent(viewParam)};
  },

  get isLoading() {
    return !this.currentViewObj || this.currentViewObj.node.hasAttribute("loading");
  },

  loadView: function(aViewId) {
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
  replaceView: function(aViewId) {
    if (aViewId == this.currentViewId)
      return;

    var state = {
      view: aViewId,
      previousView: null
    };
    gHistory.replaceState(state);
    this.loadViewInternal(aViewId, null, state);
  },

  loadInitialView: function(aViewId) {
    var state = {
      view: aViewId,
      previousView: null
    };
    gHistory.replaceState(state);

    this.loadViewInternal(aViewId, null, state);
    this.initialViewSelected = true;
    notifyInitialized();
  },

  loadViewInternal: function(aViewId, aPreviousView, aState) {
    var view = this.parseViewId(aViewId);

    if (!view.type || !(view.type in this.viewObjects))
      throw new Error("Invalid view: " + view.type);

    var viewObj = this.viewObjects[view.type];
    if (!viewObj.node)
      throw new Error("Root node doesn't exist for '" + view.type + "' view");

    if (this.currentViewObj && aViewId != aPreviousView) {
      try {
        let canHide = this.currentViewObj.hide();
        if (canHide === false)
          return;
        this.viewPort.selectedPanel.removeAttribute("loading");
      } catch (e) {
        // this shouldn't be fatal
        Cu.reportError(e);
      }
    }

    gCategories.select(aViewId, aPreviousView);

    this.currentViewId = aViewId;
    this.currentViewObj = viewObj;

    this.viewPort.selectedPanel = this.currentViewObj.node;
    this.viewPort.selectedPanel.setAttribute("loading", "true");
    this.currentViewObj.node.focus();

    if (aViewId == aPreviousView)
      this.currentViewObj.refresh(view.param, ++this.currentViewRequest, aState);
    else
      this.currentViewObj.show(view.param, ++this.currentViewRequest, aState);
  },

  // Moves back in the document history and removes the current history entry
  popState: function(aCallback) {
    this.viewChangeCallback = aCallback;
    gHistory.popState();
  },

  notifyViewChanged: function() {
    this.viewPort.selectedPanel.removeAttribute("loading");

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
      isEnabled: function() {
        return gHistory.canGoBack;
      },
      doCommand: function() {
        gHistory.back();
      }
    },

    cmd_forward: {
      isEnabled: function() {
        return gHistory.canGoForward;
      },
      doCommand: function() {
        gHistory.forward();
      }
    },

    cmd_restartApp: {
      isEnabled: function() true,
      doCommand: function() {
        let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                         createInstance(Ci.nsISupportsPRBool);
        Services.obs.notifyObservers(cancelQuit, "quit-application-requested",
                                     "restart");
        if (cancelQuit.data)
          return; // somebody canceled our quit request

        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                         getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eAttemptQuit |  Ci.nsIAppStartup.eRestart);
      }
    },

    cmd_enableCheckCompatibility: {
      isEnabled: function() true,
      doCommand: function() {
        AddonManager.checkCompatibility = true;
      }
    },

    cmd_enableUpdateSecurity: {
      isEnabled: function() true,
      doCommand: function() {
        AddonManager.checkUpdateSecurity = true;
      }
    },

    cmd_pluginCheck: {
      isEnabled: function() true,
      doCommand: function() {
        openURL(Services.urlFormatter.formatURLPref("plugins.update.url"));
      }
    },

    cmd_toggleAutoUpdateDefault: {
      isEnabled: function() true,
      doCommand: function() {
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
      isEnabled: function() true,
      doCommand: function() {
        AddonManager.getAllAddons(function(aAddonList) {
          aAddonList.forEach(function(aAddon) {
            if ("applyBackgroundUpdates" in aAddon)
              aAddon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;
          });
        });
      }
    },

    cmd_goToDiscoverPane: {
      isEnabled: function() {
        return gDiscoverView.enabled;
      },
      doCommand: function() {
        gViewController.loadView("addons://discover/");
      }
    },

    cmd_goToRecentUpdates: {
      isEnabled: function() true,
      doCommand: function() {
        gViewController.loadView("addons://updates/recent");
      }
    },

    cmd_goToAvailableUpdates: {
      isEnabled: function() true,
      doCommand: function() {
        gViewController.loadView("addons://updates/available");
      }
    },

    cmd_showItemDetails: {
      isEnabled: function(aAddon) {
        return !!aAddon && (gViewController.currentViewObj != gDetailView);
      },
      doCommand: function(aAddon, aScrollToPreferences) {
        gViewController.loadView("addons://detail/" +
                                 encodeURIComponent(aAddon.id) +
                                 (aScrollToPreferences ? "/preferences" : ""));
      }
    },

    cmd_findAllUpdates: {
      inProgress: false,
      isEnabled: function() !this.inProgress,
      doCommand: function() {
        this.inProgress = true;
        gViewController.updateCommand("cmd_findAllUpdates");
        document.getElementById("updates-noneFound").hidden = true;
        document.getElementById("updates-progress").hidden = false;
        document.getElementById("updates-manualUpdatesFound-btn").hidden = true;

        var pendingChecks = 0;
        var numUpdated = 0;
        var numManualUpdates = 0;
        var restartNeeded = false;
        var self = this;

        function updateStatus() {
          if (pendingChecks > 0)
            return;

          self.inProgress = false;
          gViewController.updateCommand("cmd_findAllUpdates");
          document.getElementById("updates-progress").hidden = true;
          gUpdatesView.maybeRefresh();

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
          onDownloadFailed: function() {
            pendingChecks--;
            updateStatus();
          },
          onInstallFailed: function() {
            pendingChecks--;
            updateStatus();
          },
          onInstallEnded: function(aInstall, aAddon) {
            pendingChecks--;
            numUpdated++;
            if (isPending(aInstall.existingAddon, "upgrade"))
              restartNeeded = true;
            updateStatus();
          }
        };

        var updateCheckListener = {
          onUpdateAvailable: function(aAddon, aInstall) {
            gEventManager.delegateAddonEvent("onUpdateAvailable",
                                             [aAddon, aInstall]);
            if (AddonManager.shouldAutoUpdate(aAddon)) {
              aInstall.addListener(updateInstallListener);
              aInstall.install();
            } else {
              pendingChecks--;
              numManualUpdates++;
              updateStatus();
            }
          },
          onNoUpdateAvailable: function(aAddon) {
            pendingChecks--;
            updateStatus();
          },
          onUpdateFinished: function(aAddon, aError) {
            gEventManager.delegateAddonEvent("onUpdateFinished",
                                             [aAddon, aError]);
          }
        };

        AddonManager.getAddonsByTypes(null, function(aAddonList) {
          aAddonList.forEach(function(aAddon) {
            if (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE) {
              pendingChecks++;
              aAddon.findUpdates(updateCheckListener,
                                 AddonManager.UPDATE_WHEN_USER_REQUESTED);
            }
          });

          if (pendingChecks == 0)
            updateStatus();
        });
      }
    },

    cmd_findItemUpdates: {
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return hasPermission(aAddon, "upgrade");
      },
      doCommand: function(aAddon) {
        var listener = {
          onUpdateAvailable: function(aAddon, aInstall) {
            gEventManager.delegateAddonEvent("onUpdateAvailable",
                                             [aAddon, aInstall]);
            if (AddonManager.shouldAutoUpdate(aAddon))
              aInstall.install();
          },
          onNoUpdateAvailable: function(aAddon) {
            gEventManager.delegateAddonEvent("onNoUpdateAvailable",
                                             [aAddon]);
          }
        };
        gEventManager.delegateAddonEvent("onCheckingUpdate", [aAddon]);
        aAddon.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
      }
    },

    cmd_showItemPreferences: {
      isEnabled: function(aAddon) {
        if (!aAddon || !aAddon.isActive || !aAddon.optionsURL)
          return false;
        if (gViewController.currentViewObj == gDetailView &&
            aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE) {
          return false;
        }
        return true;
      },
      doCommand: function(aAddon) {
        if (aAddon.optionsType == AddonManager.OPTIONS_TYPE_INLINE) {
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
      isEnabled: function(aAddon) {
        // XXXunf This may be applicable to install items too. See bug 561260
        return !!aAddon;
      },
      doCommand: function(aAddon) {
        var aboutURL = aAddon.aboutURL;
        if (aboutURL)
          openDialog(aboutURL, "", "chrome,centerscreen,modal", aAddon);
        else
          openDialog("chrome://mozapps/content/extensions/about.xul",
                     "", "chrome,centerscreen,modal", aAddon);
      }
    },

    cmd_enableItem: {
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return hasPermission(aAddon, "enable");
      },
      doCommand: function(aAddon) {
        aAddon.userDisabled = false;
      },
      getTooltip: function(aAddon) {
        if (!aAddon)
          return "";
        if (aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_ENABLE)
          return gStrings.ext.GetStringFromName("enableAddonRestartRequiredTooltip");
        return gStrings.ext.GetStringFromName("enableAddonTooltip");
      }
    },

    cmd_disableItem: {
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return hasPermission(aAddon, "disable");
      },
      doCommand: function(aAddon) {
        aAddon.userDisabled = true;
      },
      getTooltip: function(aAddon) {
        if (!aAddon)
          return "";
        if (aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_DISABLE)
          return gStrings.ext.GetStringFromName("disableAddonRestartRequiredTooltip");
        return gStrings.ext.GetStringFromName("disableAddonTooltip");
      }
    },

    cmd_installItem: {
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return aAddon.install && aAddon.install.state == AddonManager.STATE_AVAILABLE;
      },
      doCommand: function(aAddon) {
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
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return !!aAddon.purchaseURL;
      },
      doCommand: function(aAddon) {
        openURL(aAddon.purchaseURL);
      }
    },

    cmd_uninstallItem: {
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return hasPermission(aAddon, "uninstall");
      },
      doCommand: function(aAddon) {
        if (gViewController.currentViewObj != gDetailView) {
          aAddon.uninstall();
          return;
        }

        gViewController.popState(function() {
          gViewController.currentViewObj.getListItemForID(aAddon.id).uninstall();
        });
      },
      getTooltip: function(aAddon) {
        if (!aAddon)
          return "";
        if (aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL)
          return gStrings.ext.GetStringFromName("uninstallAddonRestartRequiredTooltip");
        return gStrings.ext.GetStringFromName("uninstallAddonTooltip");
      }
    },

    cmd_cancelUninstallItem: {
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return isPending(aAddon, "uninstall");
      },
      doCommand: function(aAddon) {
        aAddon.cancelUninstall();
      }
    },

    cmd_installFromFile: {
      isEnabled: function() true,
      doCommand: function() {
        const nsIFilePicker = Ci.nsIFilePicker;
        var fp = Cc["@mozilla.org/filepicker;1"]
                   .createInstance(nsIFilePicker);
        fp.init(window,
                gStrings.ext.GetStringFromName("installFromFile.dialogTitle"),
                nsIFilePicker.modeOpenMultiple);
        try {
          fp.appendFilter(gStrings.ext.GetStringFromName("installFromFile.filterName"),
                          "*.xpi;*.jar");
          fp.appendFilters(nsIFilePicker.filterAll);
        } catch (e) { }

        if (fp.show() != nsIFilePicker.returnOK)
          return;

        var files = fp.files;
        var installs = [];

        function buildNextInstall() {
          if (!files.hasMoreElements()) {
            if (installs.length > 0) {
              // Display the normal install confirmation for the installs
              AddonManager.installAddonsFromWebpage("application/x-xpinstall",
                                                    window, null, installs);
            }
            return;
          }

          var file = files.getNext();
          AddonManager.getInstallForFile(file, function(aInstall) {
            installs.push(aInstall);
            buildNextInstall();
          });
        }

        buildNextInstall();
      }
    },

    cmd_cancelOperation: {
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return aAddon.pendingOperations != AddonManager.PENDING_NONE;
      },
      doCommand: function(aAddon) {
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
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return ("contributionURL" in aAddon && aAddon.contributionURL);
      },
      doCommand: function(aAddon) {
        openURL(aAddon.contributionURL);
      }
    }
  },

  supportsCommand: function(aCommand) {
    return (aCommand in this.commands);
  },

  isCommandEnabled: function(aCommand) {
    if (!this.supportsCommand(aCommand))
      return false;
    var addon = this.currentViewObj.getSelectedAddon();
    return this.commands[aCommand].isEnabled(addon);
  },

  updateCommands: function() {
    // wait until the view is initialized
    if (!this.currentViewObj)
      return;
    var addon = this.currentViewObj.getSelectedAddon();
    for (let commandId in this.commands)
      this.updateCommand(commandId, addon);
  },

  updateCommand: function(aCommandId, aAddon) {
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

  doCommand: function(aCommand, aAddon) {
    if (!this.supportsCommand(aCommand))
      return;
    var cmd = this.commands[aCommand];
    if (!aAddon)
      aAddon = this.currentViewObj.getSelectedAddon();
    if (!cmd.isEnabled(aAddon))
      return;
    cmd.doCommand(aAddon);
  },

  onEvent: function() {}
};

function openOptionsInTab(optionsURL) {
  var mainWindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShellTreeItem)
                         .rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindow); 
  if ("switchToTabHavingURI" in mainWindow) {
    mainWindow.switchToTabHavingURI(optionsURL, true);
    return true;
  }
  return false;
}

function formatDate(aDate) {
  return Cc["@mozilla.org/intl/scriptabledateformat;1"]
           .getService(Ci.nsIScriptableDateFormat)
           .FormatDate("",
                       Ci.nsIScriptableDateFormat.dateFormatLong,
                       aDate.getFullYear(),
                       aDate.getMonth() + 1,
                       aDate.getDate()
                       );
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

  const UISTATE_ORDER = ["enabled", "pendingDisable", "pendingUninstall",
                         "disabled"];

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
      else
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

  elements.forEach(function(aElement) {
    aList.appendChild(aElement);
  });
}

function getAddonsAndInstalls(aType, aCallback) {
  let addons = null, installs = null;
  let types = (aType != null) ? [aType] : null;

  AddonManager.getAddonsByTypes(types, function(aAddonsList) {
    addons = aAddonsList;
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
        !listitem.isPending("uninstall"))
      items.push(listitem.mAddon);
    listitem = listitem.nextSibling;
  }

  items.forEach(function(aAddon) { aAddon.uninstall(); });
}

var gCategories = {
  node: null,
  _search: null,

  initialize: function() {
    this.node = document.getElementById("categories");
    this._search = this.get("addons://search/");

    var types = AddonManager.addonTypes;
    for (var type in types)
      this.onTypeAdded(types[type]);

    AddonManager.addTypeListener(this);

    try {
      this.node.value = Services.prefs.getCharPref(PREF_UI_LASTCATEGORY);
    } catch (e) { }

    // If there was no last view or no existing category matched the last view
    // then the list will default to selecting the search category and we never
    // want to show that as the first view so switch to the default category
    if (!this.node.selectedItem || this.node.selectedItem == this._search)
      this.node.value = VIEW_DEFAULT;

    var self = this;
    this.node.addEventListener("select", function() {
      self.maybeHideSearch();
      gViewController.loadView(self.node.selectedItem.value);
    }, false);

    this.node.addEventListener("click", function(aEvent) {
      var selectedItem = self.node.selectedItem;
      if (aEvent.target.localName == "richlistitem" &&
          aEvent.target == selectedItem) {
        var viewId = selectedItem.value;

        if (gViewController.parseViewId(viewId).type == "search") {
          viewId += encodeURIComponent(gHeader.searchQuery);
        }

        gViewController.loadView(viewId);
      }
    }, false);
  },

  shutdown: function() {
    AddonManager.removeTypeListener(this);
  },

  _insertCategory: function(aId, aName, aView, aPriority, aStartHidden) {
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

    var node = this.node.firstChild;
    while (node = node.nextSibling) {
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
      if (String.localeCompare(aName, node.getAttribute("name")) < 0)
        break;
    }

    this.node.insertBefore(category, node);
  },

  _removeCategory: function(aId) {
    var category = document.getElementById("category-" + aId);
    if (!category)
      return;

    // If this category is currently selected then switch to the default view
    if (this.node.selectedItem == category)
      gViewController.replaceView(VIEW_DEFAULT);

    this.node.removeChild(category);
  },

  onTypeAdded: function(aType) {
    // Ignore types that we don't have a view object for
    if (!(aType.viewType in gViewController.viewObjects))
      return;

    var aViewId = "addons://" + aType.viewType + "/" + aType.id;

    var startHidden = false;
    if (aType.flags & AddonManager.TYPE_UI_HIDE_EMPTY) {
      var prefName = PREF_UI_TYPE_HIDDEN.replace("%TYPE%", aType.id);
      try {
        startHidden = Services.prefs.getBoolPref(prefName);
      }
      catch (e) {
        // Default to hidden
        startHidden = true;
      }

      var self = this;
      gPendingInitializations++;
      getAddonsAndInstalls(aType.id, function(aAddonsList, aInstallsList) {
        var hidden = (aAddonsList.length == 0 && aInstallsList.length == 0);
        var item = self.get(aViewId);

        // Don't load view that is becoming hidden
        if (hidden && aViewId == gViewController.currentViewId)
          gViewController.loadView(VIEW_DEFAULT);

        item.hidden = hidden;
        Services.prefs.setBoolPref(prefName, hidden);

        if (aAddonsList.length > 0 || aInstallsList.length > 0) {
          notifyInitialized();
          return;
        }

        gEventManager.registerInstallListener({
          onDownloadStarted: function(aInstall) {
            this._maybeShowCategory(aInstall);
          },

          onInstallStarted: function(aInstall) {
            this._maybeShowCategory(aInstall);
          },

          onInstallEnded: function(aInstall, aAddon) {
            this._maybeShowCategory(aAddon);
          },

          onExternalInstall: function(aAddon, aExistingAddon, aRequiresRestart) {
            this._maybeShowCategory(aAddon);
          },

          _maybeShowCategory: function(aAddon) {
            if (aType.id == aAddon.type) {
              self.get(aViewId).hidden = false;
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

  onTypeRemoved: function(aType) {
    this._removeCategory(aType.id);
  },

  get selected() {
    return this.node.selectedItem ? this.node.selectedItem.value : null;
  },

  select: function(aId, aPreviousView) {
    var view = gViewController.parseViewId(aId);
    if (view.type == "detail" && aPreviousView) {
      aId = aPreviousView;
      view = gViewController.parseViewId(aPreviousView);
    }

    Services.prefs.setCharPref(PREF_UI_LASTCATEGORY, aId);

    if (this.node.selectedItem &&
        this.node.selectedItem.value == aId) {
      this.node.selectedItem.hidden = false;
      this.node.selectedItem.disabled = false;
      return;
    }

    if (view.type == "search")
      var item = this._search;
    else
      var item = this.get(aId);

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

  get: function(aId) {
    var items = document.getElementsByAttribute("value", aId);
    if (items.length)
      return items[0];
    return null;
  },

  setBadge: function(aId, aCount) {
    let item = this.get(aId);
    if (item)
      item.badgeCount = aCount;
  },

  maybeHideSearch: function() {
    var view = gViewController.parseViewId(this.node.selectedItem.value);
    this._search.disabled = view.type != "search";
  }
};


var gHeader = {
  _search: null,
  _dest: "",

  initialize: function() {
    this._search = document.getElementById("header-search");

    this._search.addEventListener("command", function(aEvent) {
      var query = aEvent.target.value;
      if (query.length == 0)
        return false;

      gViewController.loadView("addons://search/" + encodeURIComponent(query));
    }, false);

    function updateNavButtonVisibility() {
      var shouldShow = gHeader.shouldShowNavButtons;
      document.getElementById("back-btn").hidden = !shouldShow;
      document.getElementById("forward-btn").hidden = !shouldShow;
    }

    window.addEventListener("focus", function(aEvent) {
      if (aEvent.target == window)
        updateNavButtonVisibility();
    }, false);

    updateNavButtonVisibility();
  },

  focusSearchBox: function() {
    this._search.focus();
  },

  onKeyPress: function(aEvent) {
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
      var style = outerWin.getComputedStyle(node, "");
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

  initialize: function() {
    if (Services.prefs.getPrefType(PREF_DISCOVERURL) == Services.prefs.PREF_INVALID) {
      this.enabled = false;
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

    var self = this;

    function setURL(aURL) {
      try {
        self.homepageURL = Services.io.newURI(aURL, null, null);
      } catch (e) {
        self.showError();
        notifyInitialized();
        return;
      }

      self._browser.homePage = self.homepageURL.spec;
      self._browser.addProgressListener(self);

      if (self.loaded)
        self._loadURL(self.homepageURL.spec, false, notifyInitialized);
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
      aAddons.forEach(function(aAddon) {
        var prefName = PREF_GETADDONS_CACHE_ID_ENABLED.replace("%ID%",
                                                               aAddon.id);
        try {
          if (!Services.prefs.getBoolPref(prefName))
            return;
        } catch (e) { }
        list[aAddon.id] = {
          name: aAddon.name,
          version: aAddon.version,
          type: aAddon.type,
          userDisabled: aAddon.userDisabled,
          isCompatible: aAddon.isCompatible,
          isBlocklisted: aAddon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED
        }
      });

      setURL(url + "#" + JSON.stringify(list));
    });
  },

  destroy: function() {
    try {
      this._browser.removeProgressListener(this);
    }
    catch (e) {
      // Ignore the case when the listener wasn't already registered
    }
  },

  show: function(aParam, aRequest, aState, aIsRefresh) {
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
  
  canRefresh: function() {
    if (this._browser.currentURI &&
        this._browser.currentURI.spec == this._browser.homePage)
      return false;
    return true;
  },

  refresh: function(aParam, aRequest, aState) {
    this.show(aParam, aRequest, aState, true);
  },

  hide: function() { },

  showError: function() {
    this.node.selectedPanel = this._error;
  },

  _loadURL: function(aURL, aKeepHistory, aCallback) {
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

  onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {
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

  onSecurityChange: function(aWebProgress, aRequest, aState) {
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

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    let transferStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                        Ci.nsIWebProgressListener.STATE_IS_REQUEST |
                        Ci.nsIWebProgressListener.STATE_TRANSFERRING;
    // Once transferring begins show the content
    if (aStateFlags & transferStart)
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

    listeners.forEach(function(aListener) {
      aListener();
    });
  },

  onProgressChange: function() { },
  onStatusChange: function() { },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  getSelectedAddon: function() null
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

  initialize: function() {
    this.node = document.getElementById("search-view");
    this._filter = document.getElementById("search-filter-radiogroup");
    this._sorters = document.getElementById("search-sorters");
    this._sorters.handler = this;
    this._loading = document.getElementById("search-loading");
    this._listBox = document.getElementById("search-list");
    this._emptyNotice = document.getElementById("search-list-empty");
    this._allResultsLink = document.getElementById("search-allresults-link");

    var self = this;
    this._listBox.addEventListener("keydown", function(aEvent) {
      if (aEvent.keyCode == aEvent.DOM_VK_ENTER ||
          aEvent.keyCode == aEvent.DOM_VK_RETURN) {
        var item = self._listBox.selectedItem;
        if (item)
          item.showInDetailView();
      }
    }, false);

    this._filter.addEventListener("command", function() self.updateView(), false);
  },

  shutdown: function() {
    if (AddonRepository.isSearching)
      AddonRepository.cancelSearch();
  },

  get isSearching() {
    return this._pendingSearches > 0;
  },

  show: function(aQuery, aRequest) {
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
      this._listBox.removeChild(this._listBox.firstChild);

    var self = this;
    gCachedAddons = {};
    this._pendingSearches = 2;
    this._sorters.setSort("relevancescore", false);

    var elements = [];

    function createSearchResults(aObjsList, aIsInstall, aIsRemote) {
      aObjsList.forEach(function(aObj, aIndex) {
        let score = aObjsList.length - aIndex;
        if (!aIsRemote && aQuery.length > 0) {
          score = self.getMatchScore(aObj, aQuery);
          if (score == 0)
            return;
        }

        let item = createItem(aObj, aIsInstall, aIsRemote);
        item.setAttribute("relevancescore", score);
        if (aIsRemote) {
          gCachedAddons[aObj.id] = aObj;
          if (aObj.purchaseURL)
            self._sorters.showprice = true;
        }

        elements.push(item);
      });
    }

    function finishSearch(createdCount) {
      if (elements.length > 0) {
        sortElements(elements, [self._sorters.sortBy], self._sorters.ascending);
        elements.forEach(function(aElement) {
          self._listBox.insertBefore(aElement, self._listBox.lastChild);
        });
        self.updateListAttributes();
      }

      self._pendingSearches--;
      self.updateView();

      if (!self.isSearching)
        gViewController.notifyViewChanged();
    }

    getAddonsAndInstalls(null, function(aAddons, aInstalls) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      createSearchResults(aAddons, false, false);
      createSearchResults(aInstalls, true, false);
      finishSearch();
    });

    var maxRemoteResults = 0;
    try {
      maxRemoteResults = Services.prefs.getIntPref(PREF_MAXRESULTS);
    } catch(e) {}

    if (maxRemoteResults <= 0) {
      finishSearch(0);
      return;
    }

    AddonRepository.searchAddons(aQuery, maxRemoteResults, {
      searchFailed: function() {
        if (gViewController && aRequest != gViewController.currentViewRequest)
          return;

        self._lastRemoteTotal = 0;

        // XXXunf Better handling of AMO search failure. See bug 579502
        finishSearch(0); // Silently fail
      },

      searchSucceeded: function(aAddonsList, aAddonCount, aTotalResults) {
        if (gViewController && aRequest != gViewController.currentViewRequest)
          return;

        if (aTotalResults > maxRemoteResults)
          self._lastRemoteTotal = aTotalResults;
        else
          self._lastRemoteTotal = 0;

        var createdCount = createSearchResults(aAddonsList, false, true);
        finishSearch(createdCount);
      }
    });
  },
  
  showLoading: function(aLoading) {
    this._loading.hidden = !aLoading;
    this._listBox.hidden = aLoading;
  },

  updateView: function() {
    var showLocal = this._filter.value == "local";
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

  hide: function() {
    gEventManager.unregisterInstallListener(this);
    doPendingUninstalls(this._listBox);
  },

  getMatchScore: function(aObj, aQuery) {
    var score = 0;
    score += this.calculateMatchScore(aObj.name, aQuery,
                                      SEARCH_SCORE_MULTIPLIER_NAME);
    score += this.calculateMatchScore(aObj.description, aQuery,
                                      SEARCH_SCORE_MULTIPLIER_DESCRIPTION);
    return score;
  },

  calculateMatchScore: function(aStr, aQuery, aMultiplier) {
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

  showEmptyNotice: function(aShow) {
    this._emptyNotice.hidden = !aShow;
    this._listBox.hidden = aShow;
  },

  showAllResultsLink: function(aTotalResults) {
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

  updateListAttributes: function() {
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

  onSortChanged: function(aSortBy, aAscending) {
    var footer = this._listBox.lastChild;
    this._listBox.removeChild(footer);

    sortList(this._listBox, aSortBy, aAscending);
    this.updateListAttributes();

    this._listBox.appendChild(footer);
  },

  onDownloadCancelled: function(aInstall) {
    this.removeInstall(aInstall);
  },

  onInstallCancelled: function(aInstall) {
    this.removeInstall(aInstall);
  },

  removeInstall: function(aInstall) {
    for (let item of this._listBox.childNodes) {
      if (item.mInstall == aInstall) {
        this._listBox.removeChild(item);
        return;
      }
    }
  },

  getSelectedAddon: function() {
    var item = this._listBox.selectedItem;
    if (item)
      return item.mAddon;
    return null;
  },

  getListItemForID: function(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.getAttribute("status") == "installed" && listitem.mAddon.id == aId)
        return listitem;
      listitem = listitem.nextSibling;
    }
  }
};


var gListView = {
  node: null,
  _listBox: null,
  _emptyNotice: null,
  _type: null,

  initialize: function() {
    this.node = document.getElementById("list-view");
    this._listBox = document.getElementById("addon-list");
    this._emptyNotice = document.getElementById("addon-list-empty");

    var self = this;
    this._listBox.addEventListener("keydown", function(aEvent) {
      if (aEvent.keyCode == aEvent.DOM_VK_ENTER ||
          aEvent.keyCode == aEvent.DOM_VK_RETURN) {
        var item = self._listBox.selectedItem;
        if (item)
          item.showInDetailView();
      }
    }, false);
  },

  show: function(aType, aRequest) {
    if (!(aType in AddonManager.addonTypes))
      throw new Error("Attempting to show unknown type " + aType);

    this._type = aType;
    this.node.setAttribute("type", aType);
    this.showEmptyNotice(false);

    while (this._listBox.itemCount > 0)
      this._listBox.removeItemAt(0);

    var self = this;
    getAddonsAndInstalls(aType, function(aAddonsList, aInstallsList) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      var elements = [];

      for (let addonItem of aAddonsList)
        elements.push(createItem(addonItem));

      for (let installItem of aInstallsList)
        elements.push(createItem(installItem, true));

      self.showEmptyNotice(elements.length == 0);
      if (elements.length > 0) {
        sortElements(elements, ["uiState", "name"], true);
        elements.forEach(function(aElement) {
          self._listBox.appendChild(aElement);
        });
      }

      gEventManager.registerInstallListener(self);
      gViewController.updateCommands();
      gViewController.notifyViewChanged();
    });
  },

  hide: function() {
    gEventManager.unregisterInstallListener(this);
    doPendingUninstalls(this._listBox);
  },

  showEmptyNotice: function(aShow) {
    this._emptyNotice.hidden = !aShow;
  },

  onSortChanged: function(aSortBy, aAscending) {
    sortList(this._listBox, aSortBy, aAscending);
  },

  onExternalInstall: function(aAddon, aExistingAddon, aRequiresRestart) {
    // The existing list item will take care of upgrade installs
    if (aExistingAddon)
      return;

    this.addItem(aAddon);
  },

  onDownloadStarted: function(aInstall) {
    this.addItem(aInstall, true);
  },

  onInstallStarted: function(aInstall) {
    this.addItem(aInstall, true);
  },

  onDownloadCancelled: function(aInstall) {
    this.removeItem(aInstall, true);
  },

  onInstallCancelled: function(aInstall) {
    this.removeItem(aInstall, true);
  },

  onInstallEnded: function(aInstall) {
    // Remove any install entries for upgrades, their status will appear against
    // the existing item
    if (aInstall.existingAddon)
      this.removeItem(aInstall, true);
  },

  addItem: function(aObj, aIsInstall) {
    if (aObj.type != this._type)
      return;

    if (aIsInstall && aObj.existingAddon)
      return;

    let prop = aIsInstall ? "mInstall" : "mAddon";
    for (let i = 0; i < this._listBox.itemCount; i++) {
      let item = this._listBox.childNodes[i];
      if (item[prop] == aObj)
        return;
    }

    let item = createItem(aObj, aIsInstall);
    this._listBox.insertBefore(item, this._listBox.firstChild);
    this.showEmptyNotice(false);
  },

  removeItem: function(aObj, aIsInstall) {
    let prop = aIsInstall ? "mInstall" : "mAddon";

    for (let i = 0; i < this._listBox.itemCount; i++) {
      let item = this._listBox.childNodes[i];
      if (item[prop] == aObj) {
        this._listBox.removeChild(item);
        this.showEmptyNotice(this._listBox.itemCount == 0);
        return;
      }
    }
  },

  getSelectedAddon: function() {
    var item = this._listBox.selectedItem;
    if (item)
      return item.mAddon;
    return null;
  },

  getListItemForID: function(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.getAttribute("status") == "installed" && listitem.mAddon.id == aId)
        return listitem;
      listitem = listitem.nextSibling;
    }
  }
};


var gDetailView = {
  node: null,
  _addon: null,
  _loadingTimer: null,
  _autoUpdate: null,

  initialize: function() {
    this.node = document.getElementById("detail-view");

    this._autoUpdate = document.getElementById("detail-autoUpdate");

    var self = this;
    this._autoUpdate.addEventListener("command", function() {
      self._addon.applyBackgroundUpdates = self._autoUpdate.value;
    }, true);
  },
  
  shutdown: function() {
    AddonManager.removeManagerListener(this);
  },

  onUpdateModeChanged: function() {
    this.onPropertyChanged(["applyBackgroundUpdates"]);
  },

  _updateView: function(aAddon, aIsRemote, aScrollToPreferences) {
    AddonManager.addManagerListener(this);
    this.clearLoading();

    this._addon = aAddon;
    gEventManager.registerAddonListener(this, aAddon.id);
    gEventManager.registerInstallListener(this);

    this.node.setAttribute("type", aAddon.type);

    // If the search category isn't selected then make sure to select the
    // correct category
    if (gCategories.selected != "addons://search/")
      gCategories.select("addons://list/" + aAddon.type);

    document.getElementById("detail-name").textContent = aAddon.name;
    var icon = aAddon.icon64URL ? aAddon.icon64URL : aAddon.iconURL;
    document.getElementById("detail-icon").src = icon ? icon : null;
    document.getElementById("detail-creator").setCreator(aAddon.creator, aAddon.homepageURL);

    var version = document.getElementById("detail-version");
    if (shouldShowVersionNumber(aAddon)) {
      version.hidden = false;
      version.value = aAddon.version;
    } else {
      version.hidden = true;
    }

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
      screenshot.hidden = false;
    } else {
      screenshot.hidden = true;
    }

    var desc = document.getElementById("detail-desc");
    desc.textContent = aAddon.description;

    var fullDesc = document.getElementById("detail-fulldesc");
    if (aAddon.fullDescription) {
      fullDesc.textContent = aAddon.fullDescription;
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
    for (var i = 0, first = true; i < gridRows.length; ++i) {
      if (first && window.getComputedStyle(gridRows[i], null).getPropertyValue("display") != "none") {
        gridRows[i].setAttribute("first-row", true);
        first = false;
      } else {
        gridRows[i].removeAttribute("first-row");
      }
    }

    this.fillSettingsRows(aScrollToPreferences);

    this.updateState();

    gViewController.updateCommands();
    gViewController.notifyViewChanged();
  },

  show: function(aAddonId, aRequest) {
    let index = aAddonId.indexOf("/preferences");
    let scrollToPreferences = false;
    if (index >= 0) {
      aAddonId = aAddonId.substring(0, index);
      scrollToPreferences = true;
    }

    var self = this;
    this._loadingTimer = setTimeout(function() {
      self.node.setAttribute("loading-extended", true);
    }, LOADING_MSG_DELAY);

    var view = gViewController.currentViewId;

    AddonManager.getAddonByID(aAddonId, function(aAddon) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      if (aAddon) {
        self._updateView(aAddon, false, scrollToPreferences);
        return;
      }

      // Look for an add-on pending install
      AddonManager.getAllInstalls(function(aInstalls) {
        for (let install of aInstalls) {
          if (install.state == AddonManager.STATE_INSTALLED &&
              install.addon.id == aAddonId) {
            self._updateView(install.addon, false);
            return;
          }
        }

        if (aAddonId in gCachedAddons) {
          self._updateView(gCachedAddons[aAddonId], true);
          return;
        }

        // This might happen due to session restore restoring us back to an
        // add-on that doesn't exist but otherwise shouldn't normally happen.
        // Either way just revert to the default view.
        gViewController.replaceView(VIEW_DEFAULT);
      });
    });
  },

  hide: function() {
    AddonManager.removeManagerListener(this);
    this.clearLoading();
    if (this._addon) {
      if (this._addon.optionsType == AddonManager.OPTIONS_TYPE_INLINE) {
        Services.obs.notifyObservers(document,
                                     "addon-options-hidden",
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

  updateState: function() {
    gViewController.updateCommands();

    var pending = this._addon.pendingOperations;
    if (pending != AddonManager.PENDING_NONE) {
      this.node.removeAttribute("notification");

      var pending = null;
      ["enable", "disable", "install", "uninstall", "upgrade"].forEach(function(aOp) {
        if (isPending(this._addon, aOp))
          pending = aOp;
      }, this);

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
      } else if (!this._addon.isCompatible) {
        this.node.setAttribute("notification", "warning");
        document.getElementById("detail-warning").textContent = gStrings.ext.formatStringFromName(
          "details.notification.incompatible",
          [this._addon.name, gStrings.brandShortName, gStrings.appVersion], 3
        );
        document.getElementById("detail-warning-link").hidden = true;
      } else if (this._addon.blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED) {
        this.node.setAttribute("notification", "warning");
        document.getElementById("detail-warning").textContent = gStrings.ext.formatStringFromName(
          "details.notification.softblocked",
          [this._addon.name], 1
        );
        var warningLink = document.getElementById("detail-warning-link");
        warningLink.value = gStrings.ext.GetStringFromName("details.notification.softblocked.link");
        warningLink.href = this._addon.blocklistURL;
        warningLink.hidden = false;
      } else if (this._addon.blocklistState == Ci.nsIBlocklistService.STATE_OUTDATED) {
        this.node.setAttribute("notification", "warning");
        document.getElementById("detail-warning").textContent = gStrings.ext.formatStringFromName(
          "details.notification.outdated",
          [this._addon.name], 1
        );
        var warningLink = document.getElementById("detail-warning-link");
        warningLink.value = gStrings.ext.GetStringFromName("details.notification.outdated.link");
        warningLink.href = Services.urlFormatter.formatURLPref("plugins.update.url");
        warningLink.hidden = false;
      } else {
        this.node.removeAttribute("notification");
      }
    }

    this.node.setAttribute("active", this._addon.isActive);
  },

  clearLoading: function() {
    if (this._loadingTimer) {
      clearTimeout(this._loadingTimer);
      this._loadingTimer = null;
    }

    this.node.removeAttribute("loading-extended");
  },

  emptySettingsRows: function () {
    var lastRow = document.getElementById("detail-downloads");
    var rows = lastRow.parentNode;
    while (lastRow.nextSibling)
      rows.removeChild(rows.lastChild);
  },

  fillSettingsRows: function (aScrollToPreferences) {
    this.emptySettingsRows();
    if (this._addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE)
      return;

    // This function removes and returns the text content of aNode without
    // removing any child elements. Removing the text nodes ensures any XBL
    // bindings apply properly.
    function stripTextNodes(aNode) {
      var text = '';
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

    var xhr = new XMLHttpRequest();
    xhr.open("GET", this._addon.optionsURL, false);
    xhr.send();

    var xml = xhr.responseXML;
    var settings = xml.querySelectorAll(":root > setting");

    var firstSetting = null;
    for (let setting of settings) {

      var desc = stripTextNodes(setting).trim();
      if (!setting.hasAttribute("desc"))
        setting.setAttribute("desc", desc);

      var type = setting.getAttribute("type");
      if (type == "file" || type == "directory")
        setting.setAttribute("fullpath", "true");

      rows.appendChild(setting);
      var visible = window.getComputedStyle(setting, null).getPropertyValue("display") != "none";
      if (!firstSetting && visible) {
        setting.setAttribute("first-row", true);
        firstSetting = setting;
      }
    }

    // Ensure the page has loaded and force the XBL bindings to be synchronously applied,
    // then notify observers.
    if (gViewController.viewPort.selectedPanel.hasAttribute("loading")) {
      gDetailView.node.addEventListener("ViewChanged", function viewChangedEventListener() {
        gDetailView.node.removeEventListener("ViewChanged", viewChangedEventListener, false);
        if (firstSetting)
          firstSetting.clientTop;
        Services.obs.notifyObservers(document, "addon-options-displayed", gDetailView._addon.id);
        if (aScrollToPreferences)
          gDetailView.scrollToPreferencesRows();
      }, false);
    } else {
      if (firstSetting)
        firstSetting.clientTop;
      Services.obs.notifyObservers(document, "addon-options-displayed", this._addon.id);
      if (aScrollToPreferences)
        gDetailView.scrollToPreferencesRows();
    }
  },

  scrollToPreferencesRows: function() {
    // We find this row, rather than remembering it from above,
    // in case it has been changed by the observers.
    let firstRow = gDetailView.node.querySelector('setting[first-row="true"]');
    if (firstRow) {
      let top = firstRow.boxObject.y;
      top -= parseInt(window.getComputedStyle(firstRow, null).getPropertyValue("margin-top"));
      
      let detailViewBoxObject = gDetailView.node.boxObject;
      top -= detailViewBoxObject.y;

      detailViewBoxObject.QueryInterface(Ci.nsIScrollBoxObject);
      detailViewBoxObject.scrollTo(0, top);
    }
  },

  getSelectedAddon: function() {
    return this._addon;
  },

  onEnabling: function() {
    this.updateState();
  },

  onEnabled: function() {
    this.updateState();
    this.fillSettingsRows();
  },

  onDisabling: function(aNeedsRestart) {
    this.updateState();
    if (!aNeedsRestart &&
        this._addon.optionsType == AddonManager.OPTIONS_TYPE_INLINE) {
      Services.obs.notifyObservers(document,
                                   "addon-options-hidden",
                                   this._addon.id);
    }
  },

  onDisabled: function() {
    this.updateState();
    this.emptySettingsRows();
  },

  onUninstalling: function() {
    this.updateState();
  },

  onUninstalled: function() {
    gViewController.popState();
  },

  onOperationCancelled: function() {
    this.updateState();
  },

  onPropertyChanged: function(aProperties) {
    if (aProperties.indexOf("applyBackgroundUpdates") != -1) {
      this._autoUpdate.value = this._addon.applyBackgroundUpdates;
      let hideFindUpdates = AddonManager.shouldAutoUpdate(this._addon);
      document.getElementById("detail-findUpdates-btn").hidden = hideFindUpdates;
    }

    if (aProperties.indexOf("appDisabled") != -1)
      this.updateState();
  },

  onExternalInstall: function(aAddon, aExistingAddon, aNeedsRestart) {
    // Only care about upgrades for the currently displayed add-on
    if (!aExistingAddon || aExistingAddon.id != this._addon.id)
      return;

    if (!aNeedsRestart)
      this._updateView(aAddon, false);
    else
      this.updateState();
  },

  onInstallCancelled: function(aInstall) {
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

  initialize: function() {
    this.node = document.getElementById("updates-view");
    this._listBox = document.getElementById("updates-list");
    this._emptyNotice = document.getElementById("updates-list-empty");
    this._sorters = document.getElementById("updates-sorters");
    this._sorters.handler = this;

    this._categoryItem = gCategories.get("addons://updates/available");

    this._updateSelected = document.getElementById("update-selected-btn");
    this._updateSelected.addEventListener("command", function() {
      gUpdatesView.installSelected();
    }, false);

    this.updateAvailableCount(true);

    AddonManager.addAddonListener(this);
    AddonManager.addInstallListener(this);
  },

  shutdown: function() {
    AddonManager.removeAddonListener(this);
    AddonManager.removeInstallListener(this);
  },

  show: function(aType, aRequest) {
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

  hide: function() {
    this._updateSelected.hidden = true;
    this._categoryItem.disabled = this._categoryItem.badgeCount == 0;
    doPendingUninstalls(this._listBox);
  },

  _showRecentUpdates: function(aRequest) {
    var self = this;
    AddonManager.getAllAddons(function(aAddonsList) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      var elements = [];
      let threshold = Date.now() - UPDATES_RECENT_TIMESPAN;
      aAddonsList.forEach(function(aAddon) {
        if (!aAddon.updateDate || aAddon.updateDate.getTime() < threshold)
          return;

        elements.push(createItem(aAddon));
      });

      self.showEmptyNotice(elements.length == 0);
      if (elements.length > 0) {
        sortElements(elements, [self._sorters.sortBy], self._sorters.ascending);
        elements.forEach(function(aElement) {
          self._listBox.appendChild(aElement);
        });
      }

      gViewController.notifyViewChanged();
    });
  },

  _showAvailableUpdates: function(aIsRefresh, aRequest) {
    /* Disable the Update Selected button so it can't get clicked
       before everything is initialized asynchronously.
       It will get re-enabled by maybeDisableUpdateSelected(). */
    this._updateSelected.disabled = true;

    var self = this;
    AddonManager.getAllInstalls(function(aInstallsList) {
      if (!aIsRefresh && gViewController && aRequest &&
          aRequest != gViewController.currentViewRequest)
        return;

      if (aIsRefresh) {
        self.showEmptyNotice(false);
        self._updateSelected.hidden = true;

        while (self._listBox.itemCount > 0)
          self._listBox.removeItemAt(0);
      }

      var elements = [];

      aInstallsList.forEach(function(aInstall) {
        if (!self.isManualUpdate(aInstall))
          return;

        let item = createItem(aInstall.existingAddon);
        item.setAttribute("upgrade", true);
        item.addEventListener("IncludeUpdateChanged", function() {
          self.maybeDisableUpdateSelected();
        }, false);
        elements.push(item);
      });

      self.showEmptyNotice(elements.length == 0);
      if (elements.length > 0) {
        self._updateSelected.hidden = false;
        sortElements(elements, [self._sorters.sortBy], self._sorters.ascending);
        elements.forEach(function(aElement) {
          self._listBox.appendChild(aElement);
        });
      }

      // ensure badge count is in sync
      self._categoryItem.badgeCount = self._listBox.itemCount;

      gViewController.notifyViewChanged();
    });
  },

  showEmptyNotice: function(aShow) {
    this._emptyNotice.hidden = !aShow;
  },

  isManualUpdate: function(aInstall, aOnlyAvailable) {
    var isManual = aInstall.existingAddon &&
                   !AddonManager.shouldAutoUpdate(aInstall.existingAddon);
    if (isManual && aOnlyAvailable)
      return isInState(aInstall, "available");
    return isManual;
  },

  maybeRefresh: function() {
    if (gViewController.currentViewId == "addons://updates/available")
      this._showAvailableUpdates(true);
    this.updateAvailableCount();
  },

  updateAvailableCount: function(aInitializing) {
    if (aInitializing)
      gPendingInitializations++;
    var self = this;
    AddonManager.getAllInstalls(function(aInstallsList) {
      var count = aInstallsList.filter(function(aInstall) {
        return self.isManualUpdate(aInstall, true);
      }).length;
      self._categoryItem.disabled = gViewController.currentViewId != "addons://updates/available" &&
                                    count == 0;
      self._categoryItem.badgeCount = count;
      if (aInitializing)
        notifyInitialized();
    });
  },
  
  maybeDisableUpdateSelected: function() {
    for (let item of this._listBox.childNodes) {
      if (item.includeUpdate) {
        this._updateSelected.disabled = false;
        return;
      }
    }
    this._updateSelected.disabled = true;
  },

  installSelected: function() {
    for (let item of this._listBox.childNodes) {
      if (item.includeUpdate)
        item.upgrade();
    }

    this._updateSelected.disabled = true;
  },

  getSelectedAddon: function() {
    var item = this._listBox.selectedItem;
    if (item)
      return item.mAddon;
    return null;
  },

  getListItemForID: function(aId) {
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.mAddon.id == aId)
        return listitem;
      listitem = listitem.nextSibling;
    }
    return null;
  },

  onSortChanged: function(aSortBy, aAscending) {
    sortList(this._listBox, aSortBy, aAscending);
  },

  onNewInstall: function(aInstall) {
    if (!this.isManualUpdate(aInstall))
      return;
    this.maybeRefresh();
  },

  onInstallStarted: function(aInstall) {
    this.updateAvailableCount();
  },

  onInstallCancelled: function(aInstall) {
    if (!this.isManualUpdate(aInstall))
      return;
    this.maybeRefresh();
  },

  onPropertyChanged: function(aAddon, aProperties) {
    if (aProperties.indexOf("applyBackgroundUpdates") != -1)
      this.updateAvailableCount();
  }
};


var gDragDrop = {
  onDragOver: function(aEvent) {
    var types = aEvent.dataTransfer.types;
    if (types.contains("text/uri-list") ||
        types.contains("text/x-moz-url") ||
        types.contains("application/x-moz-file"))
      aEvent.preventDefault();
  },

  onDrop: function(aEvent) {
    var dataTransfer = aEvent.dataTransfer;
    var urls = [];

    // Convert every dropped item into a url
    for (var i = 0; i < dataTransfer.mozItemCount; i++) {
      var url = dataTransfer.mozGetDataAt("text/uri-list", i);
      if (url) {
        urls.push(url);
        continue;
      }

      url = dataTransfer.mozGetDataAt("text/x-moz-url", i);
      if (url) {
        urls.push(url.split("\n")[0]);
        continue;
      }

      var file = dataTransfer.mozGetDataAt("application/x-moz-file", i);
      if (file) {
        urls.push(Services.io.newFileURI(file).spec);
        continue;
      }
    }

    var pos = 0;
    var installs = [];

    function buildNextInstall() {
      if (pos == urls.length) {
        if (installs.length > 0) {
          // Display the normal install confirmation for the installs
          AddonManager.installAddonsFromWebpage("application/x-xpinstall",
                                                window, null, installs);
        }
        return;
      }

      AddonManager.getInstallForURL(urls[pos++], function(aInstall) {
        installs.push(aInstall);
        buildNextInstall();
      }, "application/x-xpinstall");
    }

    buildNextInstall();

    aEvent.preventDefault();
  }
};
