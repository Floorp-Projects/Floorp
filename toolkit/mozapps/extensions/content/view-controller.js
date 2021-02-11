/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../content/customElements.js */
/* import-globals-from aboutaddonsCommon.js */
/* import-globals-from aboutaddons.js */
/* exported loadView */

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AMTelemetry",
  "resource://gre/modules/AddonManager.jsm"
);

window.addEventListener("unload", shutdown);

window.promiseInitialized = new Promise(resolve => {
  window.addEventListener("load", () => initialize(resolve), { once: true });
});

async function initialize(resolvePromiseInitialized) {
  Services.obs.addObserver(sendEMPong, "EM-ping");
  Services.obs.notifyObservers(window, "EM-loaded");

  await gViewController.initialize().then(resolvePromiseInitialized);

  // If the initial view has already been selected (by a call to loadView from
  // the above notifications) then bail out now
  if (gViewController.initialViewSelected) {
    return;
  }

  if (history.state) {
    // If there is a history state to restore then use that
    gViewController.updateState(history.state);
  } else if (!gViewController.currentViewId) {
    // Fallback to the last category or first valid category view otherwise.
    gViewController.loadInitialView(
      document.querySelector("categories-box").initialViewId
    );
  }
}

function shutdown() {
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
async function loadView(aViewId) {
  // Make sure to wait about:addons initialization before loading
  // a view triggered by external callers.
  await window.promiseInitialized;

  if (!gViewController.initialViewSelected) {
    // The caller opened the window and immediately loaded the view so it
    // should be the initial history entry

    gViewController.loadInitialView(aViewId);
  } else {
    gViewController.loadView(aViewId);
  }
}

var gViewController = {
  currentViewId: "",
  get defaultViewId() {
    if (!isDiscoverEnabled()) {
      return "addons://list/extension";
    }
    return "addons://discover/";
  },
  initialViewSelected: false,
  isLoading: true,
  // All historyEntryId values must be unique within one session, because the
  // IDs are used to map history entries to page state. It is not possible to
  // see whether a historyEntryId was used in history entries before this page
  // was loaded, so start counting from a random value to avoid collisions.
  // This is used for scroll offsets in aboutaddons.js
  nextHistoryEntryId: Math.floor(Math.random() * 2 ** 32),

  async initialize() {
    await initializeView({
      loadViewFn: async view => {
        let viewId = view.startsWith("addons://") ? view : `addons://${view}`;
        await this.loadView(viewId);
      },
      replaceWithDefaultViewFn: async () => {
        await this.replaceView(this.defaultViewId);
      },
    });

    window.addEventListener("popstate", e => {
      this.updateState(e.state);
    });
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
    history.pushState(state, "");
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
    history.replaceState(state, "");
    this.loadViewInternal(aViewId, null, state);
  },

  loadInitialView(aViewId) {
    let state = {
      view: aViewId,
      previousView: null,
      historyEntryId: ++this.nextHistoryEntryId,
    };
    history.replaceState(state, "");
    this.loadViewInternal(aViewId, null, state);
  },

  loadViewInternal(aViewId, aPreviousView, aState) {
    const view = this.parseViewId(aViewId);
    const viewTypes = ["shortcuts", "list", "detail", "updates", "discover"];

    if (!view.type || !viewTypes.includes(view.type)) {
      throw Components.Exception("Invalid view: " + view.type);
    }

    this.currentViewId = aViewId;
    this.isLoading = true;

    recordViewTelemetry(view.param);

    let promiseLoad;
    if (aViewId != aPreviousView) {
      promiseLoad = showView(view.type, view.param, aState).then(() => {
        this.isLoading = false;

        var event = document.createEvent("Events");
        event.initEvent("ViewChanged", true, true);
        document.dispatchEvent(event);
      });
    }

    this.initialViewSelected = true;

    return promiseLoad;
  },
};
