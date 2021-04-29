/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["modal"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  Log: "chrome://marionette/content/log.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

const COMMON_DIALOG = "chrome://global/content/commonDialog.xhtml";

/** @namespace */
this.modal = {
  ACTION_CLOSED: "closed",
  ACTION_OPENED: "opened",
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
    if (
      win.document.documentURI === COMMON_DIALOG &&
      win.opener &&
      win.opener === context.window
    ) {
      logger.trace("Found open window modal prompt");
      return new modal.Dialog(() => context, win);
    }
  }

  const contentBrowser = context.contentBrowser;

  // If no modal dialog has been found yet, also check for tab and content modal
  // dialogs for the current tab.
  //
  // TODO: Find an adequate implementation for Firefox on Android (bug 1708105)
  if (context.tabBrowser?.getTabDialogBox) {
    const tabDialogBox = context.tabBrowser.getTabDialogBox(contentBrowser);

    let dialogs = tabDialogBox.getTabDialogManager().dialogs;
    if (dialogs.length) {
      logger.trace("Found open tab modal prompt");
      return new modal.Dialog(() => context, dialogs[0].frameContentWindow);
    }

    dialogs = tabDialogBox.getContentDialogManager().dialogs;
    if (dialogs.length) {
      logger.trace("Found open content prompt");
      return new modal.Dialog(() => context, dialogs[0].frameContentWindow);
    }
  }

  // If no modal dialog has been found yet, check for old non SubDialog based
  // content modal dialogs. Even with those deprecated in Firefox 89 we should
  // keep supporting applications that don't have them implemented yet.
  if (context.tabBrowser?.getTabModalPromptBox) {
    const promptBox = context.tabBrowser.getTabModalPromptBox(contentBrowser);

    const prompts = promptBox.listPrompts();
    if (prompts.length) {
      logger.trace("Found open old-style content prompt");
      return new modal.Dialog(() => context, null);
    }
  }

  return null;
};

/**
 * Observer for modal and tab modal dialogs.
 *
 * @param {function(): browser.Context} curBrowserFn
 *     Function that returns the current |browser.Context|.
 *
 * @return {modal.DialogObserver}
 *     Returns instance of the DialogObserver class.
 */
modal.DialogObserver = class {
  constructor(curBrowserFn) {
    this._curBrowserFn = curBrowserFn;

    this.callbacks = new Set();
    this.register();
  }

  register() {
    Services.obs.addObserver(this, "common-dialog-loaded");
    Services.obs.addObserver(this, "tabmodal-dialog-loaded");
    Services.obs.addObserver(this, "toplevel-window-ready");

    // Register event listener for all already open windows
    for (let win of Services.wm.getEnumerator(null)) {
      win.addEventListener("DOMModalDialogClosed", this);
    }
  }

  unregister() {
    Services.obs.removeObserver(this, "common-dialog-loaded");
    Services.obs.removeObserver(this, "tabmodal-dialog-loaded");
    Services.obs.removeObserver(this, "toplevel-window-ready");

    // Unregister event listener for all open windows
    for (let win of Services.wm.getEnumerator(null)) {
      win.removeEventListener("DOMModalDialogClosed", this);
    }
  }

  cleanup() {
    this.callbacks.clear();
    this.unregister();
  }

  handleEvent(event) {
    logger.trace(`Received event ${event.type}`);

    const chromeWin = event.target.opener
      ? event.target.opener.ownerGlobal
      : event.target.ownerGlobal;

    if (chromeWin != this._curBrowserFn().window) {
      return;
    }

    this.callbacks.forEach(callback => {
      callback(modal.ACTION_CLOSED, event.target);
    });
  }

  observe(subject, topic) {
    logger.trace(`Received observer notification ${topic}`);

    const curBrowser = this._curBrowserFn();

    switch (topic) {
      // This topic is only used by the old-style content modal dialogs like
      // alert, confirm, and prompt. It can be removed when only the new
      // subdialog based content modals remain. Those will be made default in
      // Firefox 89, and this case is deprecated.
      case "tabmodal-dialog-loaded":
        const container = curBrowser.contentBrowser.closest(
          ".browserSidebarContainer"
        );
        if (!container.contains(subject)) {
          return;
        }
        this.callbacks.forEach(callback =>
          callback(modal.ACTION_OPENED, subject)
        );
        break;

      case "common-dialog-loaded":
        const modalType = subject.Dialog.args.modalType;

        if (
          modalType === Services.prompt.MODAL_TYPE_TAB ||
          modalType === Services.prompt.MODAL_TYPE_CONTENT
        ) {
          // Find the container of the dialog in the parent document, and ensure
          // it is a descendant of the same container as the current browser.
          const container = curBrowser.contentBrowser.closest(
            ".browserSidebarContainer"
          );
          if (!container.contains(subject.docShell.chromeEventHandler)) {
            return;
          }
        } else if (
          subject.ownerGlobal != curBrowser.window &&
          subject.opener?.ownerGlobal != curBrowser.window
        ) {
          return;
        }

        this.callbacks.forEach(callback =>
          callback(modal.ACTION_OPENED, subject)
        );
        break;

      case "toplevel-window-ready":
        subject.addEventListener("DOMModalDialogClosed", this);
        break;
    }
  }

  /**
   * Add dialog handler by function reference.
   *
   * @param {function} callback
   *     The handler to be added.
   */
  add(callback) {
    if (this.callbacks.has(callback)) {
      return;
    }
    this.callbacks.add(callback);
  }

  /**
   * Remove dialog handler by function reference.
   *
   * @param {function} callback
   *     The handler to be removed.
   */
  remove(callback) {
    if (!this.callbacks.has(callback)) {
      return;
    }
    this.callbacks.delete(callback);
  }

  /**
   * Returns a promise that waits for the dialog to be closed.
   */
  async dialogClosed() {
    return new Promise(resolve => {
      const dialogClosed = (action, dialog) => {
        if (action == modal.ACTION_CLOSED) {
          this.remove(dialogClosed);
          resolve();
        }
      };

      this.add(dialogClosed);
    });
  }
};

/**
 * Represents a modal dialog.
 *
 * @param {function(): browser.Context} curBrowserFn
 *     Function that returns the current |browser.Context|.
 * @param {DOMWindow} dialog
 *     DOMWindow of the dialog.
 */
modal.Dialog = class {
  constructor(curBrowserFn, dialog) {
    this.curBrowserFn_ = curBrowserFn;
    this.win_ = Cu.getWeakReference(dialog);
  }

  get curBrowser_() {
    return this.curBrowserFn_();
  }

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

  get isWindowModal() {
    return [
      Services.prompt.MODAL_TYPE_WINDOW,
      Services.prompt.MODAL_TYPE_INTERNAL_WINDOW,
    ].includes(this.args.modalType);
  }

  get ui() {
    let tm = this.tabModal;
    return tm ? tm.ui : null;
  }
};
