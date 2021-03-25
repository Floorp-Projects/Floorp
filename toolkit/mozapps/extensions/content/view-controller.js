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
  if (gViewController.currentViewId) {
    return;
  }

  if (history.state) {
    // If there is a history state to restore then use that
    gViewController.renderState(history.state);
  } else if (!gViewController.currentViewId) {
    // Fallback to the last category or first valid category view otherwise.
    gViewController.loadView(
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
async function loadView(viewId) {
  // Make sure to wait about:addons initialization before loading
  // a view triggered by external callers.
  await window.promiseInitialized;

  gViewController.loadView(viewId);
}

var gViewController = {
  currentViewId: null,
  get defaultViewId() {
    if (!isDiscoverEnabled()) {
      return "addons://list/extension";
    }
    return "addons://discover/";
  },
  isLoading: true,
  // All historyEntryId values must be unique within one session, because the
  // IDs are used to map history entries to page state. It is not possible to
  // see whether a historyEntryId was used in history entries before this page
  // was loaded, so start counting from a random value to avoid collisions.
  // This is used for scroll offsets in aboutaddons.js
  nextHistoryEntryId: Math.floor(Math.random() * 2 ** 32),

  async initialize() {
    await initializeView();

    window.addEventListener("popstate", e => {
      this.renderState(e.state);
    });
  },

  parseViewId(viewId) {
    const matchRegex = /^addons:\/\/([^\/]+)\/(.*)$/;
    const [, viewType, viewParam] = viewId.match(matchRegex) || [];
    return { type: viewType, param: decodeURIComponent(viewParam) };
  },

  loadView(viewId, replace = false) {
    viewId = viewId.startsWith("addons://") ? viewId : `addons://${viewId}`;
    if (viewId == this.currentViewId) {
      return Promise.resolve();
    }

    // Always rewrite history state instead of pushing incorrect state for initial load.
    replace = replace || !this.currentViewId;

    const state = {
      view: viewId,
      previousView: replace ? null : this.currentViewId,
      historyEntryId: ++this.nextHistoryEntryId,
    };
    if (replace) {
      history.replaceState(state, "");
    } else {
      history.pushState(state, "");
    }
    return this.renderState(state);
  },

  renderState(state) {
    const view = this.parseViewId(state.view);
    const viewTypes = ["shortcuts", "list", "detail", "updates", "discover"];

    if (!view.type || !viewTypes.includes(view.type)) {
      throw Components.Exception("Invalid view: " + view.type);
    }

    this.currentViewId = state.view;
    this.isLoading = true;

    recordViewTelemetry(view.param);

    let promiseLoad;
    if (state.view != state.previousView) {
      promiseLoad = showView(view.type, view.param, state).then(() => {
        this.isLoading = false;

        const event = document.createEvent("Events");
        event.initEvent("ViewChanged", true, true);
        document.dispatchEvent(event);
      });
    }
    return promiseLoad;
  },

  resetState() {
    return this.loadView(this.defaultViewId, true);
  },
};
