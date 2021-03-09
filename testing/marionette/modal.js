/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["modal"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "chrome://marionette/content/log.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

const COMMON_DIALOG = "chrome://global/content/commonDialog.xhtml";

const isFirefox = () =>
  Services.appinfo.ID == "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";

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
      return new modal.Dialog(() => context, Cu.getWeakReference(win));
    }
  }

  // If no modal dialog has been found, also check if there is an open
  // tab modal dialog present for the current tab.
  // TODO: Find an adequate implementation for Fennec.
  if (context.tab && context.tabBrowser.getTabModalPromptBox) {
    let contentBrowser = context.contentBrowser;
    let promptManager = context.tabBrowser.getTabModalPromptBox(contentBrowser);
    let prompts = promptManager.listPrompts();

    if (prompts.length) {
      return new modal.Dialog(() => context, null);
    }
  }

  // No dialog found yet, check the TabDialogBox.
  // This is for prompts that are shown in SubDialogs in the browser chrome.
  if (context.tab && context.tabBrowser.getTabDialogBox) {
    let contentBrowser = context.contentBrowser;
    let dialogManager = context.tabBrowser
      .getTabDialogBox(contentBrowser)
      .getTabDialogManager();
    let dialogs = dialogManager._dialogs.filter(
      dialog => dialog._openedURL === COMMON_DIALOG
    );

    if (dialogs.length) {
      return new modal.Dialog(
        () => context,
        Cu.getWeakReference(dialogs[0]._frame.contentWindow)
      );
    }
  }

  return null;
};

/**
 * Observer for modal and tab modal dialogs.
 *
 * @return {modal.DialogObserver}
 *     Returns instance of the DialogObserver class.
 */
modal.DialogObserver = class {
  constructor() {
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

    let chromeWin = event.target.opener
      ? event.target.opener.ownerGlobal
      : event.target.ownerGlobal;

    let targetRef = Cu.getWeakReference(event.target);

    this.callbacks.forEach(callback => {
      callback(modal.ACTION_CLOSED, targetRef, chromeWin);
    });
  }

  observe(subject, topic) {
    logger.trace(`Received observer notification ${topic}`);

    switch (topic) {
      case "common-dialog-loaded":
      case "tabmodal-dialog-loaded":
        let chromeWin = subject.opener
          ? subject.opener.ownerGlobal
          : subject.ownerGlobal;

        // Always keep a weak reference to the current dialog
        let targetRef = Cu.getWeakReference(subject);

        this.callbacks.forEach(callback => {
          callback(modal.ACTION_OPENED, targetRef, chromeWin);
        });
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
   *
   * @param {window} win
   *     The window containing the modal dialog to close.
   */
  async dialogClosed(win) {
    return new Promise(resolve => {
      const dialogClosed = (action, dialog, window) => {
        if (action == modal.ACTION_CLOSED && window == win) {
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
 * @param {nsIWeakReference=} winRef
 *     A weak reference to the current |ChromeWindow|.
 */
modal.Dialog = class {
  constructor(curBrowserFn, winRef = undefined) {
    this.curBrowserFn_ = curBrowserFn;
    this.win_ = winRef;
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

  get ui() {
    let tm = this.tabModal;
    return tm ? tm.ui : null;
  }
};
