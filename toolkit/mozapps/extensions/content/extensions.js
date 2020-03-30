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

var gViewDefault = "addons://discover/";

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

  if (!isDiscoverEnabled()) {
    gViewDefault = "addons://list/extension";
  }

  // Support focusing the search bar from the XUL document.
  document.addEventListener("keypress", e => {
    getHtmlBrowser()
      .contentDocument.querySelector("search-addons")
      .handleEvent(e);
  });

  gViewController.initialize();
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
  }
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
  gViewController.shutdown();
  Services.obs.removeObserver(sendEMPong, "EM-ping");
}

function sendEMPong(aSubject, aTopic, aData) {
  Services.obs.notifyObservers(window, "EM-pong");
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
    this.viewObjects.discover = htmlView("discover");

    for (let type in this.viewObjects) {
      let view = this.viewObjects[type];
      view.initialize();
    }

    gCategories.initialize();

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

var gCategories = {
  initialize() {
    gPendingInitializations++;
    promiseHtmlBrowserLoaded().then(async browser => {
      await browser.contentWindow.customElements.whenDefined("categories-box");
      let categoriesBox = browser.contentDocument.getElementById("categories");
      await categoriesBox.promiseInitialized;
      notifyInitialized();
    });
  },
};

const htmlViewOpts = {
  loadViewFn(view) {
    let viewId = view.startsWith("addons://") ? view : `addons://${view}`;
    gViewController.loadView(viewId);
  },
  loadInitialViewFn(viewId) {
    gViewController.loadInitialView(viewId);
  },
  replaceWithDefaultViewFn() {
    gViewController.replaceView(gViewDefault);
  },
  get shouldLoadInitialView() {
    // Let the HTML document load the view if `loadView` hasn't been called
    // externally and we don't have history to refresh from.
    return !gViewController.currentViewId && !window.history.state;
  },
};

// View wrappers for the HTML version of about:addons. These delegate to an
// HTML browser that renders the actual views.
let htmlBrowser;
let _htmlBrowserLoaded;
function getHtmlBrowser() {
  if (!htmlBrowser) {
    gPendingInitializations++;
    htmlBrowser = document.getElementById("html-view-browser");
    htmlBrowser.loadURI(
      "chrome://mozapps/content/extensions/aboutaddons.html",
      {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      }
    );
    _htmlBrowserLoaded = new Promise(resolve =>
      htmlBrowser.addEventListener("load", function loadListener() {
        if (htmlBrowser.contentWindow.location.href != "about:blank") {
          htmlBrowser.removeEventListener("load", loadListener);
          resolve();
        }
      })
    ).then(() => {
      htmlBrowser.contentWindow.initialize(htmlViewOpts);
      notifyInitialized();
    });
  }
  return htmlBrowser;
}

async function promiseHtmlBrowserLoaded() {
  // Call getHtmlBrowser() first to ensure _htmlBrowserLoaded has been defined.
  let browser = getHtmlBrowser();
  await _htmlBrowserLoaded;
  return browser;
}

function htmlView(type) {
  return {
    _browser: null,
    node: null,

    initialize() {
      this._browser = getHtmlBrowser();
      this.node = this._browser.closest("#html-view");
    },

    async show(param, request, state) {
      await promiseHtmlBrowserLoaded();
      await this._browser.contentWindow.show(type, param, state);
      gViewController.notifyViewChanged();
    },

    async hide() {
      await promiseHtmlBrowserLoaded();
      return this._browser.contentWindow.hide();
    },

    getSelectedAddon() {
      return null;
    },
  };
}
