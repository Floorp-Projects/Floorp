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


const PREF_DISCOVERURL = "extensions.webservice.discoverURL";

const LOADING_MSG_DELAY = 100;

const SEARCH_SCORE_MULTIPLIER_NAME = 2;
const SEARCH_SCORE_MULTIPLIER_DESCRIPTION = 2;

const SEARCH_SCORE_MATCH_WHOLEWORD = 1;
const SEARCH_SCORE_MATCH_WORDBOUNDRY = 0.6;
const SEARCH_SCORE_MATCH_SUBSTRING = 0.3;

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


window.addEventListener("load",  initialize, false);
window.addEventListener("unload",  shutdown, false);

var gPendingInitializations = 1;
__defineGetter__("gIsInitializing", function() gPendingInitializations > 0);

function initialize() {
  gCategories.initialize();
  gHeader.initialize();
  gViewController.initialize();
  gEventManager.initialize();

  var view = VIEW_DEFAULT;
  if (gCategories.node.selectedItem &&
      gCategories.node.selectedItem.id != "category-search")
    view = gCategories.node.selectedItem.value;

  if ("arguments" in window && window.arguments.length > 0) {
    if ("view" in window.arguments[0])
      view = window.arguments[0].view;
  }

  gViewController.loadView(view);
  notifyInitialized();
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
  gEventManager.shutdown();
  gViewController.shutdown();
}

// Used by external callers to load a specific view into the manager
function loadView(url) {
  gViewController.loadView(url);
}

var gEventManager = {
  _listeners: {},
  _installListeners: [],

  initialize: function() {
    var self = this;
    ["onEnabling", "onEnabled", "onDisabling", "onDisabled", "onUninstalling",
     "onUninstalled", "onInstalled", "onOperationCancelled",
     "onUpdateAvailable", "onUpdateFinished",
     "onCompatibilityUpdateAvailable"].forEach(function(aEvent) {
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
  },

  shutdown: function() {
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
    var install = aParams[0];
    if (install.existingAddon) {
      // install is an update
      let addon = install.existingAddon;
      this.delegateAddonEvent(aEvent, [addon].concat(aParams));
      return;
    }

    this.delegateNewInstallEvent(aEvent, aParams);
  },

  delegateNewInstallEvent: function(aEvent, aParams) {
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
  }
};


var gViewController = {
  viewPort: null,
  currentViewId: "",
  currentViewObj: null,
  currentViewRequest: 0,
  previousViewId: "",
  viewObjects: {},

  initialize: function() {
    this.viewPort = document.getElementById("view-port");

    this.viewObjects["search"] = gSearchView;
    this.viewObjects["discover"] = gDiscoverView;
    this.viewObjects["list"] = gListView;
    this.viewObjects["detail"] = gDetailView;

    for each (let view in this.viewObjects)
      view.initialize();

    window.controllers.appendController(this);
  },

  shutdown: function() {
    this.currentViewRequest = 0;
  },

  parseViewId: function(aViewId) {
    var matchRegex = /^addons:\/\/([^\/]+)\/(.*)$/;
    var [,viewType, viewParam] = aViewId.match(matchRegex) || [];
    return {type: viewType, param: decodeURIComponent(viewParam)};
  },

  get isLoading() {
    return this.currentViewObj.node.hasAttribute("loading");
  },

  loadView: function(aViewId) {
    if (aViewId == this.currentViewId)
      return;

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

    gCategories.select(aViewId);

    this.previousViewId = this.currentViewId;

    this.currentViewId = aViewId;
    this.currentViewObj = viewObj;

    this.viewPort.selectedPanel = this.currentViewObj.node;
    this.viewPort.selectedPanel.setAttribute("loading", "true");
    this.currentViewObj.show(view.param, ++this.currentViewRequest);
  },

  notifyViewChanged: function() {
    this.viewPort.selectedPanel.removeAttribute("loading");
    var event = document.createEvent("Events");
    event.initEvent("ViewChanged", true, true);
    this.currentViewObj.node.dispatchEvent(event);
  },

  commands: {
    cmd_restartApp: {
      isEnabled: function() true,
      doCommand: function() {
        Application.restart();
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

    cmd_showItemDetails: {
      isEnabled: function(aAddon) {
        return !!aAddon;
      },
      doCommand: function(aAddon) {
        gViewController.loadView("addons://detail/" +
                                 encodeURIComponent(aAddon.id));
      }
    },

    cmd_findAllUpdates: {
      isEnabled: function() true,
      doCommand: function() {
        document.getElementById("updates-checkNow").hidden = true;
        document.getElementById("updates-noneFound").hidden = true;
        document.getElementById("updates-progress").hidden = false;

        var pendingChecks = 0;
        var numUpdated = 0;
        var restartNeeded = false;

        function updateStatus() {
          if (pendingChecks > 0)
            return;
          document.getElementById("updates-progress").hidden = true;

          if (numUpdated == 0) {
            document.getElementById("updates-checkNow").hidden = false;
            document.getElementById("updates-noneFound").hidden = false;
            return;
          }

          if (restartNeeded) {
            document.getElementById("updates-downloaded").hidden = false;
            document.getElementById("updates-restart").hidden = false;
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
            aInstall.addListener(updateInstallListener);
            aInstall.install();
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
      }
    },

    cmd_uninstallItem: {
      isEnabled: function(aAddon) {
        if (!aAddon)
          return false;
        return hasPermission(aAddon, "uninstall");
      },
      doCommand: function(aAddon) {
        aAddon.uninstall();
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
    for (let commandId in this.commands) {
      let cmd = document.getElementById(commandId);
      cmd.setAttribute("disabled", !this.commands[commandId].isEnabled(addon));
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


function createItem(aObj, aIsInstall, aRequiresRestart) {
  let item = document.createElement("richlistitem");

  item.setAttribute("class", "addon");
  item.setAttribute("name", aObj.name);
  item.setAttribute("type", aObj.type);

  if (aIsInstall) {
    item.mInstall = aObj;
    item.setAttribute("status", "installing");
  } else if (aRequiresRestart) {
    item.mAddon = aObj;
    item.setAttribute("status", "installing");
  } else {
    item.mAddon = aObj;

    if (isPending(aObj, "uninstall"))
      item.setAttribute("status", "uninstalled");
    else
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
  }
  return item;
}

function getAddonsAndInstalls(aType, aCallback) {
  var addonTypes = null, installTypes = null;
  if (aType != null) {
    addonTypes = [aType];
    installTypes = [aType];
    if (aType == "extension") {
      addonTypes.push("bootstrapped");
      installTypes = addonTypes.concat("");
    }
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

    var maybeHidden = ["addons://list/locale", "addons://list/searchengine"];
    gPendingInitializations += maybeHidden.length;
    maybeHidden.forEach(function(aId) {
      var type = gViewController.parseViewId(aId).param;
      getAddonsAndInstalls(type, function(aAddonsList, aInstallsList) {
        if (aAddonsList.length > 0 || aInstallsList.length > 0) {
          self.get(aId).hidden = false;
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

  select: function(aId) {
    if (this.node.selectedItem &&
        this.node.selectedItem.value == aId)
      return;

    var view = gViewController.parseViewId(aId);
    if (view.type == "detail")
      return;

    if (view.type == "search")
      var item = this._search;
    else
      var item = this.get(aId);

    if (item) {
      item.hidden = false;
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
  _name: null,
  _link: null,
  _dest: "",

  initialize: function() {
    this._name = document.getElementById("header-name");
    this._link = document.getElementById("header-link");
    this._search = document.getElementById("header-search");

    var self = this;
    this._link.addEventListener("command", function() {
      gViewController.loadView(gViewController.previousViewId);
    }, false);

    this._search.addEventListener("command", function(aEvent) {
      var query = aEvent.target.value;
      gViewController.loadView("addons://search/" + encodeURIComponent(query));
    }, false);

    this.setName("");
  },

  setName: function(aName) {
    this._name.value = aName;
    this._name.hidden = false;
    this._link.hidden = true;
  },

  showBackButton: function() {
    this._link.label = gStrings.ext.formatStringFromName("header-goBack",
                                                         [this._name.value], 1);
    this._name.hidden = true;
    this._link.hidden = false;
  },

  get searchQuery() {
    return this._search.value;
  },

  set searchQuery(aQuery) {
    this._search.value = aQuery;
  }
};


var gDiscoverView = {
  node: null,
  enabled: true,
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

      gDiscoverView._browser.homePage = url + "#" + JSON.stringify(list);
      notifyInitialized();
    });
  },

  show: function() {
    gHeader.setName(gStrings.ext.GetStringFromName("header-discover"));
    // load content only if we're not already showing something on AMO
    // XXXunf should only be comparing hostname. bug 557698
    if (this._browser.currentURI.spec.indexOf(this._browser.homePage) == -1)
      this._browser.goHome();

    gViewController.updateCommands();
    gViewController.notifyViewChanged();
  },

  hide: function() { },

  getSelectedAddon: function() null
};


var gSearchView = {
  node: null,
  _sorters: null,
  _listBox: null,
  _emptyNotice: null,

  initialize: function() {
    this.node = document.getElementById("search-view");
    this._sorters = document.getElementById("search-sorters");
    this._sorters.handler = this;
    this._listBox = document.getElementById("search-list");
    this._emptyNotice = document.getElementById("search-list-empty");

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

  show: function(aQuery, aRequest) {
    gHeader.setName(gStrings.ext.GetStringFromName("header-search"));
    this.showEmptyNotice(false);

    gHeader.searchQuery = aQuery;
    aQuery = aQuery.trim().toLocaleLowerCase();

    while (this._listBox.lastChild.localName == "richlistitem")
      this._listBox.removeChild(this._listBox.lastChild);

    var self = this;
    AddonManager.getAddonsByTypes(null, function(aAddonsList) {
      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      var elementCount = 0;
      for (let i = 0; i < aAddonsList.length; i++) {
        let addon = aAddonsList[i];
        let score = 0;
        if (aQuery.length > 0) {
          score = self.getMatchScore(addon, aQuery);
          if (score == 0)
            continue;
        }

        let item = createItem(addon);
        item.setAttribute("relevancescore", score);
        self._listBox.appendChild(item);
        elementCount++;
      }

      if (elementCount > 0)
        self.onSortChanged("relevancescore", false);
      else
        self.showEmptyNotice(true);

      gViewController.updateCommands();
      gViewController.notifyViewChanged();
    });
  },

  hide: function() { },

  getMatchScore: function(aAddon, aQuery) {
    var score = 0;
    score += this.calculateMatchScore(aAddon.name, aQuery,
                                      SEARCH_SCORE_MULTIPLIER_NAME);
    score += this.calculateMatchScore(aAddon.description, aQuery,
                                      SEARCH_SCORE_MULTIPLIER_DESCRIPTION);
    return score;
  },

  calculateMatchScore: function(aStr, aQuery, aMultiplier) {
    var score = 0;
    if (!aStr || aQuery.length == 0)
      return score;

    aStr = aStr.trim().toLocaleLowerCase();
    var haystack = aStr.split(/\W+/);
    var needles = aQuery.split(/\W+/);

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

  onSortChanged: function(aSortBy, aAscending) {
    var header = this._listBox.firstChild;
    this._listBox.removeChild(header);

    var hints = aAscending ? "ascending" : "descending";
    if (INTEGER_FIELDS.indexOf(aSortBy) >= 0)
      hints += " integer";

    var sortService = Cc["@mozilla.org/xul/xul-sort-service;1"].
                      getService(Ci.nsIXULSortService);
    sortService.sort(this._listBox, aSortBy, hints);

    this._listBox.insertBefore(header, this._listBox.firstChild);
  },

  getSelectedAddon: function() {
    var item = this._listBox.selectedItem;
    if (item)
      return item.mAddon;
    return null;
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
    gHeader.setName(gStrings.ext.GetStringFromName("header-" + aType));
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
    // the event manager ensures that upgrades are filtered out
    var item = createItem(aInstall, true);
    this._listBox.insertBefore(item, this._listBox.firstChild);
  },

  onExternalInstall: function(aAddon, aExistingAddon, aRequiresRestart) {
    if (this._types.indexOf(aAddon.type) == -1)
      return;

    var item = createItem(aAddon, false, aRequiresRestart);
    this._listBox.insertBefore(item, this._listBox.firstChild);
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
  }
};


var gDetailView = {
  node: null,
  _addon: null,
  _loadingTimer: null,

  _notificationContainer: null,
  _notificationText: null,

  initialize: function() {
    this.node = document.getElementById("detail-view");

    this._notificationContainer = document.getElementById("detail-notification");
    this._notificationText = document.getElementById("detail-notification-text");

    var self = this;
    var autoUpdate = document.getElementById("detail-autoUpdate");
    autoUpdate.addEventListener("command", function() {
      self._addon.applyBackgroundUpdates = autoUpdate.checked;
    }, true);
  },

  show: function(aAddonId, aRequest) {
    var self = this;
    this._loadingTimer = setTimeout(function() {
      self.node.setAttribute("loading-extended", true);
    }, LOADING_MSG_DELAY);
    gHeader.showBackButton();

    var view = gViewController.currentViewId;

    AddonManager.getAddonByID(aAddonId, function(aAddon) {
      self.clearLoading();

      if (gViewController && aRequest != gViewController.currentViewRequest)
        return;

      self._addon = aAddon;
      gEventManager.registerAddonListener(self, aAddon.id);

      self.node.setAttribute("type", aAddon.type);

      document.getElementById("detail-name").value = aAddon.name;
      document.getElementById("detail-icon").src = aAddon.iconURL ?
                                                     aAddon.iconURL :
                                                     "";
      document.getElementById("detail-name").value = aAddon.name;
      document.getElementById("detail-creator").setCreator(aAddon.creator,
                                     aAddon.creatorURL || aAddon.homepageURL);
      document.getElementById("detail-homepage").href = aAddon.homepageURL;
      document.getElementById("detail-version").value = aAddon.version;

      var dateUpdated = document.getElementById("detail-dateUpdated");
      if (aAddon.updateDate) {
        var date = formatDate(aAddon.updateDate);
        dateUpdated.value = gStrings.ext.formatStringFromName("dateUpdated",
                                                              [date], 1);
      }
      dateUpdated.hidden = !aAddon.updateDate;

      var desc = document.getElementById("detail-desc");
      desc.textContent = aAddon.description;

      document.getElementById("detail-autoUpdate").checked = aAddon.applyBackgroundUpdates;
      var canUpdate = hasPermission(aAddon, "upgrade");
      document.getElementById("detail-autoUpdate").hidden = !canUpdate;
      document.getElementById("detail-findUpdates").hidden = !canUpdate;
      document.getElementById("detail-prefs").hidden = !aAddon.optionsURL;

      self.updateState();

      gViewController.updateCommands();
      gViewController.notifyViewChanged();
    });
  },

  hide: function() {
    this.clearLoading();
    gEventManager.unregisterAddonListener(this, this._addon.id);
    this._addon = null;
  },

  updateState: function() {
    gViewController.updateCommands();

    var showAsActive = this._addon.isActive;
    var showUndo = false;
    var notificationMsg = null;
    if (isPending(this._addon, "enable")) {
      showAsActive = true;
      notificationMsg = "restartToEnable";
    } else if (isPending(this._addon, "disable")) {
      showAsActive = false;
      notificationMsg = "restartToDisable";
    } else if (isPending(this._addon, "uninstall")) {
      showUndo = true;
      notificationMsg = "restartToUninstall";
    } else if (isPending(this._addon, "upgrade")) {
      notificationMsg = "restartToUpgrade";
    }

    this.node.setAttribute("active", showAsActive);
    document.getElementById("detail-cancel-uninstall").hidden = !showUndo;

    if (notificationMsg) {
      var text = gStrings.ext.formatStringFromName(notificationMsg,
                                                   [this._addon.name,
                                                    gStrings.brandShortName],
                                                   2);
      this._notificationText.value = text;
    }
    this._notificationContainer.hidden = !notificationMsg;
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
    gViewController.loadView(gViewController.previousViewId);
  },

  onOperationCancelled: function() {
    this.updateState();
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
