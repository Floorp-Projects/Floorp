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
 * The Original Code is the Extension Manager UI.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blair McBride <bmcbride@mozilla.com>
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
const PREF_BACKGROUND_UPDATE = "extensions.update.enabled";
const PREF_CHECK_COMPATIBILITY = "extensions.checkCompatibility";
const PREF_CHECK_UPDATE_SECURITY = "extensions.checkUpdateSecurity";
const PREF_AUTOUPDATE_DEFAULT = "extensions.update.autoUpdateDefault";

const BRANCH_REGEXP = /^([^\.]+\.[0-9]+[a-z]*).*/gi;

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

const VIEW_DEFAULT = "addons://list/extension";

const INTEGER_FIELDS = ["dateUpdated", "size", "relevancescore"];

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

function initialize() {
  document.removeEventListener("load", initialize, true);
  gCategories.initialize();
  gHeader.initialize();
  gViewController.initialize();
  gEventManager.initialize();
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
 * A wrapper around the HTML5 session history service that continues to work
 * even if the window has session history disabled.
 * Without session history it currently only tracks the previous state so
 * the popState function works.
 */
var gHistory = {
  states: [],

  pushState: function(aState) {
    try {
      window.history.pushState(aState, document.title);
    }
    catch(e) {
      while (this.states.length > 1)
        this.states.shift();
      this.states.push(aState);
    }
  },

  replaceState: function(aState) {
    try {
      window.history.replaceState(aState, document.title);
    }
    catch (e) {
      this.states.pop();
      this.states.push(aState);
    }
  },

  popState: function() {
    // If there are no cached states then the session history must be working
    if (this.states.length == 0) {
      window.addEventListener("popstate", function(event) {
        window.removeEventListener("popstate", arguments.callee, true);
        // TODO To ensure we can't go forward again we put an additional entry
        // for the current state into the history. Ideally we would just strip
        // the history but there doesn't seem to be a way to do that. Bug 590661
        window.history.pushState(event.state, document.title);
      }, true);
      window.history.back();
    } else {
      if (this.states.length < 2)
        throw new Error("Cannot popState from this view");

      this.states.pop();
      let state = this.states[this.states.length - 1];
      gViewController.statePopped({ state: state });
    }
  },
}

var gEventManager = {
  _listeners: {},
  _installListeners: [],
  checkCompatibilityPref: "",

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
    AddonManager.addInstallListener(this);
    AddonManager.addAddonListener(this);
    
    var version = Services.appinfo.version.replace(BRANCH_REGEXP, "$1");
    this.checkCompatibilityPref = PREF_CHECK_COMPATIBILITY + "." + version;

    Services.prefs.addObserver(this.checkCompatibilityPref, this, false);
    Services.prefs.addObserver(PREF_CHECK_UPDATE_SECURITY, this, false);
    Services.prefs.addObserver(PREF_AUTOUPDATE_DEFAULT, this, false);

    this.refreshGlobalWarning();
    this.refreshAutoUpdateDefault();

    var contextMenu = document.getElementById("addonitem-popup");
    contextMenu.addEventListener("popupshowing", function() {
      var addon = gViewController.currentViewObj.getSelectedAddon();
      contextMenu.setAttribute("addontype", addon.type);
      
      var menuSep = document.getElementById("addonitem-menuseparator");
      var countEnabledMenuCmds = 0;
      for (var i = 0; i < contextMenu.children.length; i++) {
        if (contextMenu.children[i].nodeName == "menuitem" && 
          gViewController.isCommandEnabled(contextMenu.children[i].command)) {
            countEnabledMenuCmds++;
        }
      }
      
      // with only one menu item, we hide the menu separator
      menuSep.hidden = (countEnabledMenuCmds <= 1);
      
    }, false);
  },

  shutdown: function() {
    Services.prefs.removeObserver(this.checkCompatibilityPref, this);
    Services.prefs.removeObserver(PREF_CHECK_UPDATE_SECURITY, this);
    Services.prefs.removeObserver(PREF_AUTOUPDATE_DEFAULT, this, false);

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
    for (let i = 0; i < listeners.length; i++) {
      let listener = listeners[i];
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

    for (let i = 0; i < this._installListeners.length; i++) {
      let listener = this._installListeners[i];
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

    var checkUpdateSecurity = true;
    var checkUpdateSecurityDefault = true;
    try {
      checkUpdateSecurity = Services.prefs.getBoolPref(PREF_CHECK_UPDATE_SECURITY);
    } catch(e) { }
    try {
      var defaultBranch = Services.prefs.getDefaultBranch("");
      checkUpdateSecurityDefault = defaultBranch.getBoolPref(PREF_CHECK_UPDATE_SECURITY);
    } catch(e) { }
    if (checkUpdateSecurityDefault && !checkUpdateSecurity) {
      page.setAttribute("warning", "updatesecurity");
      return;
    }

    var checkCompatibility = true;
    try {
      checkCompatibility = Services.prefs.getBoolPref(this.checkCompatibilityPref);
    } catch(e) { }
    if (!checkCompatibility) {
      page.setAttribute("warning", "checkcompatibility");
      return;
    }
    
    page.removeAttribute("warning");
  },
  
  refreshAutoUpdateDefault: function() {
    var defaultEnable = true;
    try {
      defaultEnable = Services.prefs.getBoolPref(PREF_AUTOUPDATE_DEFAULT);
    } catch(e) { }
    document.getElementById("utils-autoUpdateDefault").setAttribute("checked",
                                                                    defaultEnable);
    document.getElementById("utils-resetAddonUpdatesToAutomatic").hidden = !defaultEnable;
    document.getElementById("utils-resetAddonUpdatesToManual").hidden = defaultEnable;
  },
  
  observe: function(aSubject, aTopic, aData) {
    switch (aData) {
    case this.checkCompatibilityPref:
    case PREF_CHECK_UPDATE_SECURITY:
      this.refreshGlobalWarning();
      break;
    case PREF_AUTOUPDATE_DEFAULT:
      this.refreshAutoUpdateDefault();
      break;
    }
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
                            gViewController.statePopped.bind(gViewController),
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
  },

  statePopped: function(e) {
    // If this is a navigation to a previous state then load that state
    if (e.state) {
      this.loadViewInternal(e.state.view, e.state.previousView);
      return;
    }

    // If the initial view has already been selected (by a call to loadView) then
    // bail out now
    if (this.initialViewSelected)
      return;

    // Otherwise load the default view
    var view = VIEW_DEFAULT;
    if (gCategories.node.selectedItem &&
        gCategories.node.selectedItem.id != "category-search")
      view = gCategories.node.selectedItem.value;

    if ("arguments" in window && window.arguments.length > 0) {
      if ("view" in window.arguments[0])
        view = window.arguments[0].view;
    }

    this.loadInitialView(view);
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
    if (aViewId == this.currentViewId)
      return;

    gHistory.pushState({
      view: aViewId,
      previousView: this.currentViewId
    }, document.title);
    this.loadViewInternal(aViewId, this.currentViewId);
  },

  loadInitialView: function(aViewId) {
    gHistory.replaceState({
      view: aViewId,
      previousView: null
    }, document.title);

    this.loadViewInternal(aViewId, null);
    this.initialViewSelected = true;
    notifyInitialized();
  },

  loadViewInternal: function(aViewId, aPreviousView) {
    var view = this.parseViewId(aViewId);

    if (!view.type || !(view.type in this.viewObjects))
      throw new Error("Invalid view: " + view.type);

    var viewObj = this.viewObjects[view.type];
    if (!viewObj.node)
      throw new Error("Root node doesn't exist for '" + view.type + "' view");

    if (this.currentViewObj) {
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
    this.currentViewObj.show(view.param, ++this.currentViewRequest);
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
        return window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .canGoBack;
      },
      doCommand: function() {
        window.history.back();
      }
    },

    cmd_forward: {
      isEnabled: function() {
        return window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .canGoForward;
      },
      doCommand: function() {
        window.history.forward();
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
        Services.prefs.clearUserPref(gEventManager.checkCompatibilityPref);
      }
    },

    cmd_enableUpdateSecurity: {
      isEnabled: function() true,
      doCommand: function() {
        Services.prefs.clearUserPref(PREF_CHECK_UPDATE_SECURITY);
      }
    },

    cmd_toggleAutoUpdateDefault: {
      isEnabled: function() true,
      doCommand: function() {
        var oldValue = true;
        try {
          oldValue = Services.prefs.getBoolPref(PREF_AUTOUPDATE_DEFAULT);
        } catch(e) { }
        Services.prefs.setBoolPref(PREF_AUTOUPDATE_DEFAULT, !oldValue);
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
      doCommand: function(aAddon) {
        gViewController.loadView("addons://detail/" +
                                 encodeURIComponent(aAddon.id));
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
        var autoUpdateDefault = AddonManager.autoUpdateDefault;
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
            if (shouldAutoUpdate(aAddon, autoUpdateDefault)) {
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
            if (shouldAutoUpdate(aAddon))
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
        if (!aAddon)
          return false;
        return aAddon.isActive && !!aAddon.optionsURL;
      },
      doCommand: function(aAddon) {
        var optionsURL = aAddon.optionsURL;
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
          openDialog(aboutURL, "", "chrome,centerscreen,modal");
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
                                                    this, null, installs);
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


function shouldAutoUpdate(aAddon, aDefault) {
  if (!("applyBackgroundUpdates" in aAddon))
    return false;
  if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_ENABLE)
    return true;
  if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_DISABLE)
    return false;
  return aDefault !== undefined ? aDefault : AddonManager.autoUpdateDefault;
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

  // The XUL sort service only supports 32 bit integers so we strip the
  // milliseconds to make this small enough
  if (aObj.updateDate)
    item.setAttribute("dateUpdated", aObj.updateDate.getTime() / 1000);

  if (aObj.size)
    item.setAttribute("size", aObj.size);
  return item;
}

function getAddonsAndInstalls(aType, aCallback) {
  var addonTypes = null, installTypes = null;
  if (aType != null) {
    addonTypes = [aType];
    installTypes = [aType];
    if (aType == "extension")
      installTypes = addonTypes.concat("");
  }

  var addons = null, installs = null;

  AddonManager.getAddonsByTypes(addonTypes, function(aAddonsList) {
    addons = aAddonsList;
    if (installs != null)
      aCallback(addons, installs);
  });

  AddonManager.getInstallsByTypes(installTypes, function(aInstallsList) {
    // skip over upgrade installs and non-active installs
    installs = aInstallsList.filter(function(aInstall) {
      return !(aInstall.existingAddon ||
               aInstall.state == AddonManager.STATE_AVAILABLE);
    });

    if (addons != null)
      aCallback(addons, installs)
  });

  return {addon: addonTypes, install: installTypes};
}


var gCategories = {
  node: null,
  _search: null,
  _maybeHidden: null,

  initialize: function() {
    this.node = document.getElementById("categories");
    this._search = this.get("addons://search/");

    this.maybeHideSearch();

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

    this._maybeHidden = ["addons://list/locale", "addons://list/searchengine"];
    gPendingInitializations += this._maybeHidden.length;
    this._maybeHidden.forEach(function(aId) {
      var type = gViewController.parseViewId(aId).param;
      getAddonsAndInstalls(type, function(aAddonsList, aInstallsList) {
        var hidden = (aAddonsList.length == 0 && aInstallsList.length == 0);
        var item = self.get(aId);

        // Don't load view that is becoming hidden
        if (hidden && aId == gViewController.currentViewId)
          gViewController.loadView(VIEW_DEFAULT);

        item.hidden = hidden;

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
            if (type == aAddon.type) {
              self.get(aId).hidden = false;
              gEventManager.unregisterInstallListener(this);
            }
          }
        });

        notifyInitialized();
      });
    });
  },

  shutdown: function() {
    // Force persist of hidden state. See bug 15232
    var self = this;
    this._maybeHidden.forEach(function(aId) {
      var item = self.get(aId);
      item.setAttribute("hidden", !!item.hidden);
    });
  },

  select: function(aId, aPreviousView) {
    var view = gViewController.parseViewId(aId);
    if (view.type == "detail") {
      aId = aPreviousView;
      view = gViewController.parseViewId(aPreviousView);
    }

    if (this.node.selectedItem &&
        this.node.selectedItem.value == aId)
      return;

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
      // When supressing onselect last-selected doesn't get updated
      this.node.setAttribute("last-selected", item.id);

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
  _searching: null,
  _dest: "",

  initialize: function() {
    this._search = document.getElementById("header-search");
    this._searching = document.getElementById("header-searching");

    this._search.addEventListener("command", function(aEvent) {
      var query = aEvent.target.value;
      if (query.length == 0)
        return false;

      gViewController.loadView("addons://search/" + encodeURIComponent(query));
    }, false);
  },

  get searchQuery() {
    return this._search.value;
  },

  set searchQuery(aQuery) {
    this._search.value = aQuery;
  },

  get isSearching() {
    return this._searching.hasAttribute("active");
  },

  set isSearching(aIsSearching) {
    if (aIsSearching)
      this._searching.setAttribute("active", true);
    else
      this._searching.removeAttribute("active");
  }
};


var gDiscoverView = {
  node: null,
  enabled: true,
  // Set to true after the view is first shown. If initialization completes
  // after this then it must also load the discover homepage
  loaded: false,
  _browser: null,

  initialize: function() {
    if (Services.prefs.getPrefType(PREF_DISCOVERURL) == Services.prefs.PREF_INVALID) {
      this.enabled = false;
      gCategories.get("addons://discover/").hidden = true;
      return;
    }

    this.node = document.getElementById("discover-view");
    this._browser = document.getElementById("discover-browser");

    var url = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                .getService(Ci.nsIURLFormatter)
                .formatURLPref(PREF_DISCOVERURL);

    gPendingInitializations++;
    AddonManager.getAllAddons(function(aAddons) {
      var list = {};
      aAddons.forEach(function(aAddon) {
        list[aAddon.id] = {
          name: aAddon.name,
          version: aAddon.version,
          type: aAddon.type,
          userDisabled: aAddon.userDisabled,
          isCompatible: aAddon.isCompatible,
          isBlocklisted: aAddon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED
        }
      });

      var browser = gDiscoverView._browser;
      browser.homePage = url + "#" + JSON.stringify(list);

      if (gDiscoverView.loaded) {
        browser.addEventListener("load", function() {
          browser.removeEventListener("load", arguments.callee, true);
          notifyInitialized();
        }, true);
        browser.goHome();
      } else {
        notifyInitialized();
      }
    });
  },

  show: function() {
    if (!this.loaded) {
      this.loaded = true;

      var browser = gDiscoverView._browser;
      browser.addEventListener("load", function() {
        browser.removeEventListener("load", arguments.callee, true);
        gViewController.updateCommands();
        gViewController.notifyViewChanged();
      }, true);
      browser.goHome();
    } else {
      gViewController.updateCommands();
      gViewController.notifyViewChanged();
    }
  },

  hide: function() { },

  getSelectedAddon: function() null
};


var gCachedAddons = {};

var gSearchView = {
  node: null,
  _filter: null,
  _sorters: null,
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

    gHeader.isSearching = true;
    this.showEmptyNotice(false);
    this.showAllResultsLink(0);

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

    function createSearchResults(aObjsList, aIsInstall, aIsRemote) {
      var createdCount = 0;
      aObjsList.forEach(function(aObj) {
        let score = 0;
        if (aQuery.length > 0) {
          score = self.getMatchScore(aObj, aQuery);
          if (score == 0 && !aIsRemote)
            return;
        }

        let item = createItem(aObj, aIsInstall, aIsRemote);
        item.setAttribute("relevancescore", score);
        if (aIsRemote)
          gCachedAddons[aObj.id] = aObj;

        self._listBox.insertBefore(item, self._listBox.lastChild);
        createdCount++;
      });

      return createdCount;
    }

    function finishSearch(createdCount) {
      if (createdCount > 0)
        self.onSortChanged(self._sorters.sortBy, self._sorters.ascending);

      self._pendingSearches--;
      self.updateView();

      if (!self.isSearching)
        gViewController.notifyViewChanged();
    }

    getAddonsAndInstalls(null, function(aAddons, aInstalls) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      var createdCount = createSearchResults(aAddons, false, false);
      createdCount += createSearchResults(aInstalls, true, false);
      finishSearch(createdCount);
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

  updateView: function() {
    var showLocal = this._filter.value == "local";
    this._listBox.setAttribute("local", showLocal);
    this._listBox.setAttribute("remote", !showLocal);

    gHeader.isSearching = this.isSearching;
    if (!this.isSearching) {
      var isEmpty = true;
      var results = this._listBox.getElementsByTagName("richlistitem");
      for (let i = 0; i < results.length; i++) {
        var isRemote = (results[i].getAttribute("remote") == "true");
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

    // Uninstalling add-ons can mutate the list so find the add-ons first then
    // uninstall them
    var items = [];
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.getAttribute("pending") == "uninstall" &&
          !listitem.isPending("uninstall"))
        items.push(listitem.mAddon);
      listitem = listitem.nextSibling;
    }

    items.forEach(function(aAddon) { aAddon.uninstall(); });
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

    for (let n = 0; n < needles.length; n++) {
      for (let h = 0; h < haystack.length; h++) {
        if (haystack[h] == needles[n]) {
          // matching whole words is best
          score += SEARCH_SCORE_MATCH_WHOLEWORD;
        } else {
          let i = haystack[h].indexOf(needles[n]);
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
  },

  showAllResultsLink: function(aTotalResults) {
    if (aTotalResults == 0) {
      this._allResultsLink.hidden = true;
      return;
    }

    var linkStr = gStrings.ext.GetStringFromName("showAllSearchResults");
    linkStr = PluralForm.get(aTotalResults, linkStr);
    linkStr = linkStr.replace("#1", aTotalResults);
    this._allResultsLink.value = linkStr;

    this._allResultsLink.href = AddonRepository.getSearchURL(this._lastQuery);
    this._allResultsLink.hidden = false;
 },

  onSortChanged: function(aSortBy, aAscending) {
    var footer = this._listBox.lastChild;
    this._listBox.removeChild(footer);

    var hints = aAscending ? "ascending" : "descending";
    if (INTEGER_FIELDS.indexOf(aSortBy) >= 0)
      hints += " integer";

    var sortService = Cc["@mozilla.org/xul/xul-sort-service;1"].
                      getService(Ci.nsIXULSortService);
    sortService.sort(this._listBox, aSortBy, hints);

    this._listBox.appendChild(footer);
  },

  onDownloadCancelled: function(aInstall) {
    this.removeInstall(aInstall);
  },

  onInstallCancelled: function(aInstall) {
    this.removeInstall(aInstall);
  },

  removeInstall: function(aInstall) {
    for (let i = 0; i < this._listBox.childNodes.length; i++) {
      let item = this._listBox.childNodes[i];
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
  _sorters: null,
  _types: [],
  _installTypes: [],

  initialize: function() {
    this.node = document.getElementById("list-view");
    this._sorters = document.getElementById("list-sorters");
    this._sorters.handler = this;
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
    this.showEmptyNotice(false);

    while (this._listBox.itemCount > 0)
      this._listBox.removeItemAt(0);

    var self = this;
    var types = getAddonsAndInstalls(aType, function(aAddonsList, aInstallsList) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      for (let i = 0; i < aAddonsList.length; i++) {
        let item = createItem(aAddonsList[i]);
        self._listBox.appendChild(item);
      }

      for (let i = 0; i < aInstallsList.length; i++) {
        let item = createItem(aInstallsList[i], true);
        self._listBox.appendChild(item);
      }

      if (self._listBox.childElementCount > 0)
        self.onSortChanged(self._sorters.sortBy, self._sorters.ascending);
      else
        self.showEmptyNotice(true);

      gEventManager.registerInstallListener(self);
      gViewController.updateCommands();
      gViewController.notifyViewChanged();
    });

    this._types = types.addon;
    this._installTypes = types.install;
  },

  hide: function() {
    gEventManager.unregisterInstallListener(this);

    // Uninstalling add-ons can mutate the list so find the add-ons first then
    // uninstall them
    var items = [];
    var listitem = this._listBox.firstChild;
    while (listitem) {
      if (listitem.getAttribute("pending") == "uninstall" &&
          !listitem.isPending("uninstall"))
        items.push(listitem.mAddon);
      listitem = listitem.nextSibling;
    }

    items.forEach(function(aAddon) { aAddon.uninstall(); });
  },

  showEmptyNotice: function(aShow) {
    this._emptyNotice.hidden = !aShow;
  },

  onSortChanged: function(aSortBy, aAscending) {
    var hints = aAscending ? "ascending" : "descending";
    if (INTEGER_FIELDS.indexOf(aSortBy) >= 0)
      hints += " integer";

    var sortService = Cc["@mozilla.org/xul/xul-sort-service;1"].
                      getService(Ci.nsIXULSortService);
    sortService.sort(this._listBox, aSortBy, hints);
  },

  onNewInstall: function(aInstall) {
    // Ignore any upgrade installs
    if (aInstall.existingAddon)
      return;

    var item = createItem(aInstall, true);
    this._listBox.insertBefore(item, this._listBox.firstChild);
  },

  onExternalInstall: function(aAddon, aExistingAddon, aRequiresRestart) {
    if (this._types.indexOf(aAddon.type) == -1)
      return;

    // The existing list item will take care of upgrade installs
    if (aExistingAddon)
      return;

    var item = createItem(aAddon, false);
    this._listBox.insertBefore(item, this._listBox.firstChild);
  },

  onDownloadCancelled: function(aInstall) {
    this.removeInstall(aInstall);
  },

  onInstallCancelled: function(aInstall) {
    this.removeInstall(aInstall);
  },

  onInstallEnded: function(aInstall) {
    // Remove any install entries for upgrades, their status will appear against
    // the existing item
    if (aInstall.existingAddon)
      this.removeInstall(aInstall);
  },

  removeInstall: function(aInstall) {
    for (let i = 0; i < this._listBox.childNodes.length; i++) {
      let item = this._listBox.childNodes[i];
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


var gDetailView = {
  node: null,
  _addon: null,
  _loadingTimer: null,
  _updatePrefs: null,
  _autoUpdate: null,

  initialize: function() {
    this.node = document.getElementById("detail-view");

    this._autoUpdate = document.getElementById("detail-autoUpdate");

    var self = this;
    this._autoUpdate.addEventListener("command", function() {
      self._addon.applyBackgroundUpdates = self._autoUpdate.value;
    }, true);
    
    this._updatePrefs = Services.prefs.getBranch("extensions.update.");
    this._updatePrefs.QueryInterface(Ci.nsIPrefBranch2);
  },
  
  shutdown: function() {
    this._updatePrefs.removeObserver("", this);
    delete this._updatePrefs;
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed" && aData == "autoUpdateDefault") {
      this.onPropertyChanged(["applyBackgroundUpdates"]);
    }
  },

  _updateView: function(aAddon, aIsRemote) {
    this._updatePrefs.addObserver("", this, false);
    this.clearLoading();

    this._addon = aAddon;
    gEventManager.registerAddonListener(this, aAddon.id);
    gEventManager.registerInstallListener(this);

    this.node.setAttribute("type", aAddon.type);

    document.getElementById("detail-name").textContent = aAddon.name;
    var icon = aAddon.icon64URL ? aAddon.icon64URL : aAddon.iconURL;
    document.getElementById("detail-icon").src = icon ? icon : null;
    document.getElementById("detail-creator").setCreator(aAddon.creator, aAddon.homepageURL);

    var version = document.getElementById("detail-version");
    if (aAddon.version) {
      version.hidden = false;
      version.value = aAddon.version;
    } else {
      version.hidden = true;
    }

    var screenshot = document.getElementById("detail-screenshot");
    if (aAddon.screenshots && aAddon.screenshots.length > 0) {
      if (aAddon.screenshots[0].thumbnailURL)
        screenshot.src = aAddon.screenshots[0].thumbnailURL;
      else
        screenshot.src = aAddon.screenshots[0].url;
      screenshot.hidden = false;
    } else {
      screenshot.hidden = true;
    }

    var desc = document.getElementById("detail-desc");
    desc.textContent = aAddon.fullDescription ? aAddon.fullDescription
                                              : aAddon.description;

    var contributions = document.getElementById("detail-contributions");
    if ("contributionURL" in aAddon && aAddon.contributionURL) {
      contributions.hidden = false;
      var amount = document.getElementById("detail-contrib-suggested");
      amount.value = gStrings.ext.formatStringFromName("contributionAmount",
                                                       [aAddon.contributionAmount],
                                                       1);
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
      formatted = gStrings.dl.GetStringFromName("doneSize");
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

    var canUpdate = !aIsRemote && hasPermission(aAddon, "upgrade");
    document.getElementById("detail-updates-row").hidden = !canUpdate;

    if ("applyBackgroundUpdates" in aAddon) {
      this._autoUpdate.hidden = false;
      this._autoUpdate.value = aAddon.applyBackgroundUpdates;
      let hideFindUpdates = shouldAutoUpdate(this._addon);
      document.getElementById("detail-findUpdates-btn").hidden = hideFindUpdates;
    } else {
      this._autoUpdate.hidden = true;
      document.getElementById("detail-findUpdates-btn").hidden = false;
    }

    document.getElementById("detail-prefs-btn").hidden = !aIsRemote && !aAddon.optionsURL;
    
    var gridRows = document.querySelectorAll("#detail-grid rows row");
    for (var i = 0, first = true; i < gridRows.length; ++i) {
      if (first && window.getComputedStyle(gridRows[i], null).getPropertyValue("display") != "none") {
        gridRows[i].setAttribute("first-row", true);
        first = false;
      } else {
        gridRows[i].removeAttribute("first-row");
      }
    }

    this.updateState();

    gViewController.updateCommands();
    gViewController.notifyViewChanged();
  },

  show: function(aAddonId, aRequest) {
    var self = this;
    this._loadingTimer = setTimeout(function() {
      self.node.setAttribute("loading-extended", true);
    }, LOADING_MSG_DELAY);

    var view = gViewController.currentViewId;

    AddonManager.getAddonByID(aAddonId, function(aAddon) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      if (aAddon) {
        self._updateView(aAddon, false);
        return;
      }

      // Look for an add-on pending install
      AddonManager.getAllInstalls(function(aInstalls) {
        for (let i = 0; i < aInstalls.length; i++) {
          if (aInstalls[i].state == AddonManager.STATE_INSTALLED &&
              aInstalls[i].addon.id == aAddonId) {
            self._updateView(aInstalls[i].addon, false);
            return;
          }
        }

        if (aAddonId in gCachedAddons) {
          self._updateView(gCachedAddons[aAddonId], true);
          return;
        }

        // This case should never happen in normal operation
      });
    });
  },

  hide: function() {
    this._updatePrefs.removeObserver("", this);
    this.clearLoading();
    gEventManager.unregisterAddonListener(this, this._addon.id);
    gEventManager.unregisterInstallListener(this);
    this._addon = null;
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
        errorLink.href = Services.urlFormatter.formatURLPref("extensions.blocklist.detailsURL");
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
        warningLink.href = Services.urlFormatter.formatURLPref("extensions.blocklist.detailsURL");
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

  getSelectedAddon: function() {
    return this._addon;
  },

  onEnabling: function() {
    this.updateState();
  },

  onEnabled: function() {
    this.updateState();
  },

  onDisabling: function() {
    this.updateState();
  },

  onDisabled: function() {
    this.updateState();
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
      let hideFindUpdates = shouldAutoUpdate(this._addon);
      document.getElementById("detail-findUpdates-btn").hidden = hideFindUpdates;
    }
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
  _updatePrefs: null,
  _categoryItem: null,
  _numManualUpdaters: 0,

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

    this._updatePrefs = Services.prefs.getBranch("extensions.update.");
    this._updatePrefs.QueryInterface(Ci.nsIPrefBranch2);
    this._updatePrefs.addObserver("", this, false);
    this.updateManualUpdatersCount(true);
    this.updateAvailableCount(true);

    AddonManager.addAddonListener(this);
    AddonManager.addInstallListener(this);
  },

  shutdown: function() {
    AddonManager.removeAddonListener(this);
    AddonManager.removeInstallListener(this);
    this._updatePrefs.removeObserver("", this);
    delete this._updatePrefs;
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
  },

  _showRecentUpdates: function(aRequest) {
    var self = this;
    AddonManager.getAllAddons(function(aAddonsList) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      let threshold = Date.now() - UPDATES_RECENT_TIMESPAN;
      aAddonsList.forEach(function(aAddon) {
        if (!aAddon.updateDate || aAddon.updateDate.getTime() < threshold)
          return;

        let item = createItem(aAddon);
        self._listBox.appendChild(item);
      });

      if (self._listBox.itemCount > 0)
        self.onSortChanged(self._sorters.sortBy, self._sorters.ascending);
      else
        self.showEmptyNotice(true);

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
      if (!aIsRefresh && gViewController && aRequest != gViewController.currentViewRequest)
        return;

      if (aIsRefresh) {
        self.showEmptyNotice(false);
        self._updateSelected.hidden = true;

        while (self._listBox.itemCount > 0)
          self._listBox.removeItemAt(0);
      }

      aInstallsList.forEach(function(aInstall) {
        if (!self.isManualUpdate(aInstall))
          return;

        let item = createItem(aInstall.existingAddon);
        item.setAttribute("upgrade", true);
        item.addEventListener("IncludeUpdateChanged", function() {
          self.maybeDisableUpdateSelected();
        }, false);
        self._listBox.appendChild(item);
      });

      if (self._listBox.itemCount > 0) {
        self._updateSelected.hidden = false;
        self.onSortChanged(self._sorters.sortBy, self._sorters.ascending);
      } else {
        self.showEmptyNotice(true);
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
                   !shouldAutoUpdate(aInstall.existingAddon);
    if (isManual && aOnlyAvailable)
      return isInState(aInstall, "available");
    return isManual;
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "nsPref:changed")
      return;
    if (aData == "autoUpdateDefault")
      this.updateManualUpdatersCount();
  },

  maybeRefresh: function() {
    if (gViewController.currentViewId == "addons://updates/available") {
      this._showAvailableUpdates(true);
    } else {
      this.updateManualUpdatersCount();
      this.updateAvailableCount();
    }
  },

  maybeShowCategory: function() {
    var hide = this._numManualUpdaters == 0;
    if (this._categoryItem.disabled != hide) {
      this._categoryItem.disabled = hide;
      var event = document.createEvent("Events");
      event.initEvent("CategoryVisible", true, true);
      this._categoryItem.dispatchEvent(event);
    }
  },

  updateManualUpdatersCount: function(aInitializing) {
    if (aInitializing)
      gPendingInitializations++;
    var self = this;
    var autoUpdateDefault = AddonManager.autoUpdateDefault;
    AddonManager.getAllAddons(function(aAddonList) {
      var manualUpdaters = aAddonList.filter(function(aAddon) {
        if (!("applyBackgroundUpdates" in aAddon))
          return false;
        return !shouldAutoUpdate(aAddon, autoUpdateDefault);
      });
      self._numManualUpdaters = manualUpdaters.length;
      self.maybeShowCategory();
      if (aInitializing)
        notifyInitialized();
    });
  },

  updateAvailableCount: function(aInitializing) {
    if (aInitializing)
      gPendingInitializations++;
    var self = this;
    AddonManager.getAllInstalls(function(aInstallsList) {
      var count = aInstallsList.filter(function(aInstall) {
        return self.isManualUpdate(aInstall, true);
      }).length;
      self._categoryItem.badgeCount = count;
      if (aInitializing)
        notifyInitialized();
    });
  },
  
  maybeDisableUpdateSelected: function() {
    for (let i = 0; i < this._listBox.childNodes.length; i++) {
      let item = this._listBox.childNodes[i];
      if (item.includeUpdate) {
        this._updateSelected.disabled = false;
        return;
      }
    }
    this._updateSelected.disabled = true;
  },

  installSelected: function() {
    /* Starting the update of one item will refresh the list,
       which can cause problems while we're iterating over it.
       So we update only after we've finished iterating over the list. */
    var toUpgrade = [];
    for (let i = 0; i < this._listBox.childNodes.length; i++) {
      let item = this._listBox.childNodes[i];
      if (item.includeUpdate)
        toUpgrade.push(item);
    }
    toUpgrade.forEach(function(aItem) aItem.upgrade());
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
    var hints = aAscending ? "ascending" : "descending";
    if (INTEGER_FIELDS.indexOf(aSortBy) >= 0)
      hints += " integer";

    var sortService = Cc["@mozilla.org/xul/xul-sort-service;1"].
                      getService(Ci.nsIXULSortService);
    sortService.sort(this._listBox, aSortBy, hints);
  },

  onNewInstall: function(aInstall) {
    if (!this.isManualUpdate(aInstall))
      return;
    this.maybeRefresh();
  },

  onExternalInstall: function(aAddon) {
    if (!shouldAutoUpdate(aAddon)) {
      this._numManualUpdaters++;
      this.maybeShowCategory();
    }
  },

  onDownloadStarted: function(aInstall) {
    if (!this.isManualUpdate(aInstall))
      return;
    this.maybeRefresh();
  },

  onInstallStarted: function(aInstall) {
    if (!this.isManualUpdate(aInstall))
      return;
    this.maybeRefresh();
  },

  onInstallEnded: function(aAddon) {
    if (!shouldAutoUpdate(aAddon)) {
      this._numManualUpdaters++;
      this.maybeShowCategory();
    }
  },

  onPropertyChanged: function(aAddon, aProperties) {
    if (aProperties.indexOf("applyBackgroundUpdates") != -1)
      this.updateManualUpdatersCount();
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
          AddonManager.installAddonsFromWebpage("application/x-xpinstall", this,
                                                null, installs);
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
