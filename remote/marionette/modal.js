/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["modal"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AppInfo: "chrome://remote/content/shared/AppInfo.jsm",

  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

const COMMON_DIALOG = "chrome://global/content/commonDialog.xhtml";

/** @namespace */
const modal = {
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
      lazy.logger.trace("Found open window modal prompt");
      return new modal.Dialog(() => context, win);
    }
  }

  if (lazy.AppInfo.isAndroid) {
    const geckoViewPrompts = context.window.prompts();
    if (geckoViewPrompts.length) {
      lazy.logger.trace("Found open GeckoView prompt");
      const prompt = geckoViewPrompts[0];
      return new modal.Dialog(() => context, prompt);
    }
  }

  const contentBrowser = context.contentBrowser;

  // If no modal dialog has been found yet, also check for tab and content modal
  // dialogs for the current tab.
  //
  // TODO: Find an adequate implementation for Firefox on Android (bug 1708105)
  if (contentBrowser?.tabDialogBox) {
    let dialogs = contentBrowser.tabDialogBox.getTabDialogManager().dialogs;
    if (dialogs.length) {
      lazy.logger.trace("Found open tab modal prompt");
      return new modal.Dialog(() => context, dialogs[0].frameContentWindow);
    }

    dialogs = contentBrowser.tabDialogBox.getContentDialogManager().dialogs;

    // Even with the dialog manager handing back a dialog, the `Dialog` property
    // gets lazily added. If it's not set yet, ignore the dialog for now.
    if (dialogs.length && dialogs[0].frameContentWindow.Dialog) {
      lazy.logger.trace("Found open content prompt");
      return new modal.Dialog(() => context, dialogs[0].frameContentWindow);
    }
  }

  // If no modal dialog has been found yet, check for old non SubDialog based
  // content modal dialogs. Even with those deprecated in Firefox 89 we should
  // keep supporting applications that don't have them implemented yet.
  if (contentBrowser?.tabModalPromptBox) {
    const prompts = contentBrowser.tabModalPromptBox.listPrompts();
    if (prompts.length) {
      lazy.logger.trace("Found open old-style content prompt");
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
    Services.obs.addObserver(this, "domwindowopened");
    Services.obs.addObserver(this, "geckoview-prompt-show");
    Services.obs.addObserver(this, "tabmodal-dialog-loaded");

    // Register event listener for all already open windows
    for (let win of Services.wm.getEnumerator(null)) {
      win.addEventListener("DOMModalDialogClosed", this);
    }
  }

  unregister() {
    Services.obs.removeObserver(this, "common-dialog-loaded");
    Services.obs.removeObserver(this, "domwindowopened");
    Services.obs.removeObserver(this, "geckoview-prompt-show");
    Services.obs.removeObserver(this, "tabmodal-dialog-loaded");

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
    lazy.logger.trace(`Received event ${event.type}`);

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
    lazy.logger.trace(`Received observer notification ${topic}`);

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

      case "domwindowopened":
        subject.addEventListener("DOMModalDialogClosed", this);
        break;

      case "geckoview-prompt-show":
        for (let win of Services.wm.getEnumerator(null)) {
          const prompt = win.prompts().find(item => item.id == subject.id);
          if (prompt) {
            this.callbacks.forEach(callback =>
              callback(modal.ACTION_OPENED, prompt)
            );
            return;
          }
        }
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

  get args() {
    if (lazy.AppInfo.isAndroid) {
      return this.window.args;
    }
    let tm = this.tabModal;
    return tm ? tm.args : null;
  }

  get curBrowser_() {
    return this.curBrowserFn_();
  }

  get isOpen() {
    if (lazy.AppInfo.isAndroid) {
      return this.window !== null;
    }
    if (!this.ui) {
      return false;
    }
    return true;
  }

  get isWindowModal() {
    return [
      Services.prompt.MODAL_TYPE_WINDOW,
      Services.prompt.MODAL_TYPE_INTERNAL_WINDOW,
    ].includes(this.args.modalType);
  }

  get tabModal() {
    let win = this.window;
    if (win) {
      return win.Dialog;
    }
    return this.curBrowser_.getTabModal();
  }

  get text() {
    if (lazy.AppInfo.isAndroid) {
      return this.window.getPromptText();
    }
    return this.ui.infoBody.textContent;
  }

  get ui() {
    let tm = this.tabModal;
    return tm ? tm.ui : null;
  }

  /**
   * For Android, this returns a GeckoViewPrompter, which can be used to control prompts.
   * Otherwise, this returns the ChromeWindow associated with an open dialog window if
   * it is currently attached to the DOM.
   */
  get window() {
    if (this.win_) {
      let win = this.win_.get();
      if (win && (lazy.AppInfo.isAndroid || win.parent)) {
        return win;
      }
    }
    return null;
  }

  set text(inputText) {
    if (lazy.AppInfo.isAndroid) {
      this.window.setInputText(inputText);
    } else {
      // see toolkit/components/prompts/content/commonDialog.js
      let { loginTextbox } = this.ui;
      loginTextbox.value = inputText;
    }
  }

  accept() {
    if (lazy.AppInfo.isAndroid) {
      // GeckoView does not have a UI, so the methods are called directly
      this.window.acceptPrompt();
    } else {
      const { button0 } = this.ui;
      button0.click();
    }
  }

  dismiss() {
    if (lazy.AppInfo.isAndroid) {
      // GeckoView does not have a UI, so the methods are called directly
      this.window.dismissPrompt();
    } else {
      const { button0, button1 } = this.ui;
      (button1 ? button1 : button0).click();
    }
  }
};
