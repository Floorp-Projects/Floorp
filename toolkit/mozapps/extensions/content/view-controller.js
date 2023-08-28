/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from /toolkit/content/customElements.js */
/* import-globals-from aboutaddonsCommon.js */
/* exported loadView */

// Used by external callers to load a specific view into the manager
function loadView(viewId) {
  if (!gViewController.readyForLoadView) {
    throw new Error("loadView called before about:addons is initialized");
  }
  gViewController.loadView(viewId);
}

/**
 * Helper for saving and restoring the scroll offsets when a previously loaded
 * view is accessed again.
 */
var ScrollOffsets = {
  _key: null,
  _offsets: new Map(),
  canRestore: true,

  setView(historyEntryId) {
    this._key = historyEntryId;
    this.canRestore = true;
  },

  getPosition() {
    if (!this.canRestore) {
      return { top: 0, left: 0 };
    }
    let { scrollTop: top, scrollLeft: left } = document.documentElement;
    return { top, left };
  },

  save() {
    if (this._key) {
      this._offsets.set(this._key, this.getPosition());
    }
  },

  restore() {
    let { top = 0, left = 0 } = this._offsets.get(this._key) || {};
    window.scrollTo({ top, left, behavior: "auto" });
  },
};

var gViewController = {
  currentViewId: null,
  readyForLoadView: false,
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

    window.addEventListener("popstate", this);
    window.addEventListener("unload", this, { once: true });
    Services.obs.addObserver(this, "EM-ping");
  },

  handleEvent(e) {
    if (e.type == "popstate") {
      this.renderState(e.state);
      return;
    }

    if (e.type == "unload") {
      Services.obs.removeObserver(this, "EM-ping");
      // eslint-disable-next-line no-useless-return
      return;
    }
  },

  observe(subject, topic, data) {
    if (topic == "EM-ping") {
      this.readyForLoadView = true;
      Services.obs.notifyObservers(window, "EM-pong");
    }
  },

  notifyEMLoaded() {
    this.readyForLoadView = true;
    Services.obs.notifyObservers(window, "EM-loaded");
  },

  notifyEMUpdateCheckFinished() {
    // Notify the observer about a completed update check (currently only used in tests).
    Services.obs.notifyObservers(null, "EM-update-check-finished");
  },

  defineView(viewName, renderFunction) {
    if (this.views[viewName]) {
      throw new Error(
        `about:addons view ${viewName} should not be defined twice`
      );
    }
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

    if (!type || this.views[type] == null) {
      console.warn(`No view for ${type} ${param}, switching to default`);
      this.resetState();
      return;
    }

    ScrollOffsets.save();
    ScrollOffsets.setView(state.historyEntryId);

    this.currentViewId = state.view;
    this.isLoading = true;

    // Perform tasks before view load
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
      this.container.textContent = "";
      this.container.append(fragment);

      // Most content has been rendered at this point. The only exception are
      // recommendations in the discovery pane and extension/theme list, because
      // they rely on remote data. If loaded before, then these may be rendered
      // within one tick, so wait a frame before restoring scroll offsets.
      await new Promise(resolve => {
        window.requestAnimationFrame(() => {
          // Double requestAnimationFrame in case we reflow.
          window.requestAnimationFrame(() => {
            ScrollOffsets.restore();
            resolve();
          });
        });
      });
    } else {
      // Reset to default view if no given content
      this.resetState();
      return;
    }

    this.isLoading = false;

    document.dispatchEvent(new CustomEvent("view-loaded"));
  },

  resetState() {
    return this.loadView(this.defaultViewId, true);
  },
};
