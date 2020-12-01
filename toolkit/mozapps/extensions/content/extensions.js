/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../content/customElements.js */
/* import-globals-from aboutaddonsCommon.js */
/* exported loadView */

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AMTelemetry",
  "resource://gre/modules/AddonManager.jsm"
);

document.addEventListener("load", initialize, true);
window.addEventListener("unload", shutdown);

var gPendingInitializations = 1;
Object.defineProperty(this, "gIsInitializing", {
  get: () => gPendingInitializations > 0,
});

function initialize(event) {
  document.removeEventListener("load", initialize, true);

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

  let { currentViewId } = gViewController;
  let viewType = gViewController.parseViewId(currentViewId)?.type;
  AMTelemetry.recordViewEvent({
    view: viewType || "other",
    addon,
    type,
  });
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
  defaultViewId: "addons://discover/",
  currentViewId: "",
  isLoading: true,
  // All historyEntryId values must be unique within one session, because the
  // IDs are used to map history entries to page state. It is not possible to
  // see whether a historyEntryId was used in history entries before this page
  // was loaded, so start counting from a random value to avoid collisions.
  // This is used for scroll offsets in aboutaddons.js
  nextHistoryEntryId: Math.floor(Math.random() * 2 ** 32),
  initialViewSelected: false,

  initialize() {
    if (!isDiscoverEnabled()) {
      this.defaultViewId = "addons://list/extension";
    }

    gCategories.initialize();

    window.controllers.appendController(this);

    window.addEventListener("popstate", e => {
      this.updateState(e.state);
    });
  },

  shutdown() {
    window.controllers.removeController(this);
  },

  updateState(state) {
    this.loadViewInternal(state.view, state.previousView, state);
  },

  parseViewId(aViewId) {
    var matchRegex = /^addons:\/\/([^\/]+)\/(.*)$/;
    var [, viewType, viewParam] = aViewId.match(matchRegex) || [];
    return { type: viewType, param: decodeURIComponent(viewParam) };
  },

  loadView(aViewId) {
    if (aViewId == this.currentViewId) {
      return;
    }

    var state = {
      view: aViewId,
      previousView: this.currentViewId,
      historyEntryId: ++this.nextHistoryEntryId,
    };
    gHistory.pushState(state);
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

  loadViewInternal(aViewId, aPreviousView, aState) {
    const view = this.parseViewId(aViewId);
    const viewTypes = ["shortcuts", "list", "detail", "updates", "discover"];

    if (!view.type || !viewTypes.includes(view.type)) {
      throw Components.Exception("Invalid view: " + view.type);
    }

    if (aViewId != aPreviousView) {
      promiseHtmlBrowserLoaded()
        .then(browser => browser.contentWindow.hide())
        .catch(err => Cu.reportError(err));
    }

    this.currentViewId = aViewId;
    this.isLoading = true;

    recordViewTelemetry(view.param);

    if (aViewId != aPreviousView) {
      promiseHtmlBrowserLoaded()
        .then(browser =>
          browser.contentWindow.show(view.type, view.param, aState)
        )
        .then(() => {
          this.isLoading = false;

          var event = document.createEvent("Events");
          event.initEvent("ViewChanged", true, true);
          document.dispatchEvent(event);
        });
    }

    this.initialViewSelected = true;
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
    gViewController.replaceView(gViewController.defaultViewId);
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

// Helper method exported into the about:addons global, used to open the
// abuse report panel from outside of the about:addons page
// (e.g. triggered from the browserAction context menu).
window.openAbuseReport = ({ addonId, reportEntryPoint }) => {
  promiseHtmlBrowserLoaded().then(browser => {
    browser.contentWindow.openAbuseReport({
      addonId,
      reportEntryPoint,
    });
  });
};
