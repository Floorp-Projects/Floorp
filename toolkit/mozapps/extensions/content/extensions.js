/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../content/customElements.js */
/* import-globals-from aboutaddonsCommon.js */
/* globals ProcessingInstruction */
/* exported loadView */

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AMTelemetry",
  "resource://gre/modules/AddonManager.jsm"
);

const PREF_UI_TYPE_HIDDEN = "extensions.ui.%TYPE%.hidden";

var gViewDefault = "addons://discover/";

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
  if (!isDiscoverEnabled()) {
    gViewDefault = "addons://list/extension";
  }

  // Support focusing the search bar from the XUL document.
  document.addEventListener("keypress", e => {
    htmlBrowser.contentDocument.querySelector("search-addons").handleEvent(e);
  });

  let helpButton = document.getElementById("helpButton");
  let helpUrl =
    Services.urlFormatter.formatURLPref("app.support.baseURL") + "addons-help";
  helpButton.setAttribute("href", helpUrl);
  helpButton.addEventListener("click", () => recordLinkTelemetry("support"));

  document.getElementById("preferencesButton").addEventListener("click", e => {
    if (e.button >= 2) {
      return;
    }
    let mainWindow = window.windowRoot.ownerGlobal;
    recordLinkTelemetry("about:preferences");
    if ("switchToTabHavingURI" in mainWindow) {
      mainWindow.switchToTabHavingURI("about:preferences", true, {
        ignoreFragment: "whenComparing",
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
  gEventManager.initialize();
  Services.obs.addObserver(sendEMPong, "EM-ping");
  Services.obs.notifyObservers(window, "EM-loaded");

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
    !!window.arguments.length &&
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
  AMTelemetry.recordLinkEvent({ object: "aboutAddons", value: target, extra });
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

function isDiscoverEnabled() {
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
  },

  forward() {
    window.history.forward();
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
  },

  forward() {
    if (this.pos + 1 >= this.states.length) {
      throw Components.Exception("Cannot go forward from this point");
    }

    this.pos++;
    gViewController.updateState(this.states[this.pos]);
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
};

var gViewController = {
  viewPort: null,
  currentViewId: "",
  currentViewObj: null,
  currentViewRequest: 0,
  // All historyEntryId values must be unique within one session, because the
  // IDs are used to map history entries to page state. It is not possible to
  // see whether a historyEntryId was used in history entries before this page
  // was loaded, so start counting from a random value to avoid collisions.
  nextHistoryEntryId: Math.floor(Math.random() * 2 ** 32),
  viewObjects: {},
  viewChangeCallback: null,
  initialViewSelected: false,
  lastHistoryIndex: -1,

  initialize() {
    this.viewPort = document.getElementById("view-port");
    this.headeredViews = document.getElementById("headered-views");
    this.headeredViewsDeck = document.getElementById("headered-views-content");

    this.viewObjects.shortcuts = htmlView("shortcuts");
    this.viewObjects.list = htmlView("list");
    this.viewObjects.detail = htmlView("detail");
    this.viewObjects.updates = htmlView("updates");
    // gUpdatesView still handles when the Available Updates category is
    // shown. Include it in viewObjects so it gets initialized and shutdown.
    this.viewObjects._availableUpdatesSidebar = gUpdatesView;
    this.viewObjects.discover = htmlView("discover");

    if (!isDiscoverEnabled()) {
      gCategories.get("addons://discover/").hidden = true;
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

  loadView(aViewId) {
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

    var state = {
      view: aViewId,
      previousView: this.currentViewId,
      historyEntryId: ++this.nextHistoryEntryId,
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
      historyEntryId: ++this.nextHistoryEntryId,
    };
    gHistory.replaceState(state);
    this.loadViewInternal(aViewId, null, state);
  },

  loadInitialView(aViewId) {
    var state = {
      view: aViewId,
      previousView: null,
      historyEntryId: ++this.nextHistoryEntryId,
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

    if (aViewId == aPreviousView) {
      this.currentViewObj.refresh(
        view.param,
        ++this.currentViewRequest,
        aState
      );
    } else {
      this.currentViewObj.show(view.param, ++this.currentViewRequest, aState);
    }

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

  onEvent() {},
};

function isInState(aInstall, aState) {
  var state = AddonManager["STATE_" + aState.toUpperCase()];
  return aInstall.state == state;
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
      // This is needed for keyboard support.
      gViewController.loadView(this.node.selectedItem.value);
    });
    this.node.addEventListener("click", e => {
      let item = e.target.closest("richlistitem");
      if (item) {
        // This is needed to return from details/shortcuts back to the list view.
        gViewController.loadView(item.value);
      }
    });
  },

  shutdown() {
    AddonManager.removeTypeListener(this);
  },

  _defineCustomElement() {
    class MozCategory extends MozElements.MozRichlistitem {
      connectedCallback() {
        if (this.delayConnectedCallback()) {
          return;
        }
        this.textContent = "";
        this.appendChild(
          MozXULElement.parseXULToFragment(`
          <image class="category-icon"/>
          <label class="category-name" crop="end" flex="1"/>
          <label class="category-badge"/>
        `)
        );
        this.initializeAttributeInheritance();

        if (!this.hasAttribute("count")) {
          this.setAttribute("count", 0);
        }
      }

      static get inheritedAttributes() {
        return {
          ".category-name": "value=name",
          ".category-badge": "value=count",
        };
      }

      set badgeCount(val) {
        if (this.getAttribute("count") == val) {
          return;
        }

        this.setAttribute("count", val);
      }

      get badgeCount() {
        return this.getAttribute("count");
      }
    }

    customElements.define("addon-category", MozCategory, {
      extends: "richlistitem",
    });
  },

  _insertCategory(aId, aName, aView, aPriority, aStartHidden) {
    // If this category already exists then don't re-add it
    if (document.getElementById("category-" + aId)) {
      return;
    }

    var category = document.createXULElement("richlistitem", {
      is: "addon-category",
    });
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
        var hidden = !aAddonsList.length && !aInstallsList.length;
        var item = this.get(aViewId);

        // Don't load view that is becoming hidden
        if (hidden && aViewId == gViewController.currentViewId) {
          gViewController.loadView(gViewDefault);
        }

        item.hidden = hidden;
        Services.prefs.setBoolPref(prefName, hidden);

        if (aAddonsList.length || aInstallsList.length) {
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

// This needs to be defined before the XUL is parsed because some of the
// categories are in the XUL markup.
gCategories._defineCustomElement();

var gUpdatesView = {
  _categoryItem: null,
  isRoot: true,

  initialize() {
    this._categoryItem = gCategories.get("addons://updates/available");
    this.updateAvailableCount(true);

    AddonManager.addAddonListener(this);
    AddonManager.addInstallListener(this);
  },

  shutdown() {
    AddonManager.removeAddonListener(this);
    AddonManager.removeInstallListener(this);
  },

  show(aType, aRequest) {
    throw new Error(
      "should not get here (available updates view is in aboutaddons.js"
    );
  },

  hide() {},

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

const addonTypes = new Set([
  "extension",
  "theme",
  "plugin",
  "dictionary",
  "locale",
]);
const htmlViewOpts = {
  loadViewFn(view) {
    let viewId = view.startsWith("addons://") ? view : `addons://${view}`;
    gViewController.loadView(viewId);
  },
  replaceWithDefaultViewFn() {
    gViewController.replaceView(gViewDefault);
  },
  setCategoryFn(name) {
    if (addonTypes.has(name)) {
      gCategories.select(`addons://list/${name}`);
    }
  },
  getCurrentViewIdFn() {
    return gViewController.currentViewId;
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
      htmlBrowser.addEventListener("load", function loadListener() {
        if (htmlBrowser.contentWindow.location.href != "about:blank") {
          htmlBrowser.removeEventListener("load", loadListener);
          resolve();
        }
      })
    ).then(() => htmlBrowser.contentWindow.initialize(htmlViewOpts));
  }
  return htmlBrowser;
}

let leafViewTypes = ["detail", "shortcuts"];
function htmlView(type) {
  return {
    _browser: null,
    node: null,
    isRoot: !leafViewTypes.includes(type),

    initialize() {
      this._browser = getHtmlBrowser();
      this.node = this._browser.closest("#html-view");
    },

    async show(param, request, state) {
      await htmlBrowserLoaded;
      this.node.setAttribute("type", type);
      this.node.setAttribute("param", param);
      await this._browser.contentWindow.show(type, param, state);
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
