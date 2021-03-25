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
function loadView(viewId) {
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
  views: {},

  initialize(container) {
    this.container = container;

    window.addEventListener("popstate", e => {
      this.renderState(e.state);
    });
  },

  defineView(viewName, renderFunction) {
    this.views[viewName] = renderFunction;
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

  async renderState(state) {
    let { param, type } = this.parseViewId(state.view);

    if (!type || !this.views.hasOwnProperty(type)) {
      console.warn(`No view for ${type} ${param}, switching to default`);
      this.resetState();
      return;
    }

    this.currentViewId = state.view;
    this.isLoading = true;

    // Perform tasks before view load
    recordViewTelemetry(param);
    document.dispatchEvent(
      new CustomEvent("view-selected", {
        detail: { id: state.view, param, type },
      })
    );

    // Render the fragment
    this.container.setAttribute("current-view", type);
    let fragment = await this.views[type](param);

    // Clear and append the fragment
    if (fragment) {
      ScrollOffsets.save();
      ScrollOffsets.setView(state.historyEntryId);

      this.container.textContent = "";
      this.container.append(fragment);

      // Most content has been rendered at this point. The only exception are
      // recommendations in the discovery pane and extension/theme list, because
      // they rely on remote data. If loaded before, then these may be rendered
      // within one tick, so wait a frame before restoring scroll offsets.
      await new Promise(resolve => {
        window.requestAnimationFrame(() => {
          ScrollOffsets.restore();
          resolve();
        });
      });
    } else {
      // Reset to default view if no given content
      this.resetState();
      return;
    }

    this.isLoading = false;

    document.dispatchEvent(new CustomEvent("ViewChanged"));
  },

  resetState() {
    return this.loadView(this.defaultViewId, true);
  },
};
