/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DialogHandler"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const DIALOG_TYPES = {
  ALERT: "alert",
  BEFOREUNLOAD: "beforeunload",
  CONFIRM: "confirm",
  PROMPT: "prompt",
};

/**
 * Helper dedicated to detect and interact with browser dialogs such as `alert`,
 * `confirm` etc. The current implementation only supports tabmodal dialogs,
 * not full window dialogs.
 *
 * Emits "dialog-loaded" when a javascript dialog is opened for the current
 * browser.
 *
 * @param {BrowserElement} browser
 */
class DialogHandler {
  constructor(browser) {
    EventEmitter.decorate(this);
    this._dialog = null;
    this._browser = browser;

    this._onTabDialogLoaded = this._onTabDialogLoaded.bind(this);

    Services.obs.addObserver(this._onTabDialogLoaded, "tabmodal-dialog-loaded");
  }

  destructor() {
    this._dialog = null;
    this._pageTarget = null;

    Services.obs.removeObserver(
      this._onTabDialogLoaded,
      "tabmodal-dialog-loaded"
    );
  }

  async handleJavaScriptDialog({ accept, promptText }) {
    if (!this._dialog) {
      throw new Error("No dialog available for handleJavaScriptDialog");
    }

    const type = this._getDialogType();
    if (promptText && type === "prompt") {
      this._dialog.ui.loginTextbox.value = promptText;
    }

    const onDialogClosed = new Promise(r => {
      this._browser.addEventListener("DOMModalDialogClosed", r, {
        once: true,
      });
    });

    // 0 corresponds to the OK callback, 1 to the CANCEL callback.
    if (accept) {
      this._dialog.onButtonClick(0);
    } else {
      this._dialog.onButtonClick(1);
    }

    await onDialogClosed;

    // Resetting dialog to null here might be racy and lead to errors if the
    // content page is triggering several prompts in a row.
    // See Bug 1569578.
    this._dialog = null;
  }

  _getDialogType() {
    const { inPermitUnload, promptType } = this._dialog.args;

    if (inPermitUnload) {
      return DIALOG_TYPES.BEFOREUNLOAD;
    }

    switch (promptType) {
      case "alert":
        return DIALOG_TYPES.ALERT;
      case "confirm":
        return DIALOG_TYPES.CONFIRM;
      case "prompt":
        return DIALOG_TYPES.PROMPT;
      default:
        throw new Error("Unsupported dialog type: " + promptType);
    }
  }

  _onTabDialogLoaded(promptContainer) {
    const prompts = this._browser.tabModalPromptBox.listPrompts();
    const prompt = prompts.find(p => p.ui.promptContainer === promptContainer);

    if (!prompt) {
      // The dialog is not for the current tab.
      return;
    }

    this._dialog = prompt;
    const message = this._dialog.args.text;
    const type = this._getDialogType();

    this.emit("dialog-loaded", { message, type });
  }
}
