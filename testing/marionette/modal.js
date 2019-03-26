/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["modal"];

const COMMON_DIALOG = "chrome://global/content/commonDialog.xul";

const isFirefox = () =>
    Services.appinfo.ID == "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";

/** @namespace */
this.modal = {
  COMMON_DIALOG_LOADED: "common-dialog-loaded",
  TABMODAL_DIALOG_LOADED: "tabmodal-dialog-loaded",
  handlers: {
    "common-dialog-loaded": new Set(),
    "tabmodal-dialog-loaded": new Set(),
  },
};

/**
 * Add handler that will be called when a global- or tab modal dialogue
 * appears.
 *
 * This is achieved by installing observers for common-
 * and tab modal loaded events.
 *
 * This function is a no-op if called on any other product than Firefox.
 *
 * @param {function(Object, string)} handler
 *     The handler to be called, which is passed the
 *     subject (e.g. ChromeWindow) and the topic (one of
 *     {@code modal.COMMON_DIALOG_LOADED} or
 *     {@code modal.TABMODAL_DIALOG_LOADED}.
 */
modal.addHandler = function(handler) {
  if (!isFirefox()) {
    return;
  }

  Object.keys(this.handlers).map(topic => {
    this.handlers[topic].add(handler);
    Services.obs.addObserver(handler, topic);
  });
};

/**
 * Check for already existing modal or tab modal dialogs
 *
 * @param {browser.Context} context
 *     Reference to the browser context to check for existent dialogs.
 *
 * @return {modal.Dialog}
 *     Returns instance of the Dialog class, or `null` if no modal dialog
 *     is present.
 */
modal.findModalDialogs = function(context) {
  // First check if there is a modal dialog already present for the
  // current browser window.
  for (let win of Services.wm.getEnumerator(null)) {
    // TODO: Use BrowserWindowTracker.getTopWindow for modal dialogs without
    // an opener.
    if (win.document.documentURI === COMMON_DIALOG &&
        win.opener && win.opener === context.window) {
      return new modal.Dialog(() => context, Cu.getWeakReference(win));
    }
  }

  // If no modal dialog has been found, also check if there is an open
  // tab modal dialog present for the current tab.
  // TODO: Find an adequate implementation for Fennec.
  if (context.tab && context.tabBrowser.getTabModalPromptBox) {
    let contentBrowser = context.contentBrowser;
    let promptManager =
        context.tabBrowser.getTabModalPromptBox(contentBrowser);
    let prompts = promptManager.listPrompts();

    if (prompts.length) {
      return new modal.Dialog(() => context, null);
    }
  }

  return null;
};

/**
 * Remove modal dialogue handler by function reference.
 *
 * This function is a no-op if called on any other product than Firefox.
 *
 * @param {function} toRemove
 *     The handler previously passed to modal.addHandler which will now
 *     be removed.
 */
modal.removeHandler = function(toRemove) {
  if (!isFirefox()) {
    return;
  }

  for (let topic of Object.keys(this.handlers)) {
    let handlers = this.handlers[topic];
    for (let handler of handlers) {
      if (handler == toRemove) {
        Services.obs.removeObserver(handler, topic);
        handlers.delete(handler);
      }
    }
  }
};

/**
 * Represents the current modal dialogue.
 *
 * @param {function(): browser.Context} curBrowserFn
 *     Function that returns the current |browser.Context|.
 * @param {nsIWeakReference=} winRef
 *     A weak reference to the current |ChromeWindow|.
 */
modal.Dialog = class {
  constructor(curBrowserFn, winRef = undefined) {
    this.curBrowserFn_ = curBrowserFn;
    this.win_ = winRef;
  }

  get curBrowser_() { return this.curBrowserFn_(); }

  /**
   * Returns the ChromeWindow associated with an open dialog window if
   * it is currently attached to the DOM.
   */
  get window() {
    if (this.win_) {
      let win = this.win_.get();
      if (win && win.parent) {
        return win;
      }
    }
    return null;
  }

  get tabModal() {
    let win = this.window;
    if (win) {
      return win.Dialog;
    }
    return this.curBrowser_.getTabModal();
  }

  get args() {
    let tm = this.tabModal;
    return tm ? tm.args : null;
  }

  get ui() {
    let tm = this.tabModal;
    return tm ? tm.ui : null;
  }
};
