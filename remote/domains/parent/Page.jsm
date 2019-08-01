/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Page"];

const { DialogHandler } = ChromeUtils.import(
  "chrome://remote/content/domains/parent/page/DialogHandler.jsm"
);
const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);

class Page extends Domain {
  constructor(session) {
    super(session);

    this._onDialogLoaded = this._onDialogLoaded.bind(this);

    this.enabled = false;
  }

  destructor() {
    // Flip a flag to avoid to disable the content domain from this.disable()
    this._isDestroyed = false;
    this.disable();

    super.destructor();
  }

  // commands

  async enable() {
    if (this.enabled) {
      return;
    }

    this.enabled = true;

    const { browser } = this.session.target;
    this._dialogHandler = new DialogHandler(browser);
    this._dialogHandler.on("dialog-loaded", this._onDialogLoaded);
    await this.executeInChild("enable");
  }

  async disable() {
    if (!this.enabled) {
      return;
    }

    this._dialogHandler.destructor();
    this._dialogHandler = null;
    this.enabled = false;

    if (!this._isDestroyed) {
      // Only call disable in the content domain if we are not destroying the domain.
      // If we are destroying the domain, the content domains will be destroyed
      // independently after firing the remote:destroy event.
      await this.executeInChild("disable");
    }
  }

  bringToFront() {
    const { browser } = this.session.target;
    const navigator = browser.ownerGlobal;
    const { gBrowser } = navigator;

    // Focus the window responsible for this page.
    navigator.focus();

    // Select the corresponding tab
    gBrowser.selectedTab = gBrowser.getTabForBrowser(browser);
  }

  /**
   * Interact with the currently opened JavaScript dialog (alert, confirm,
   * prompt) for this page. This will always close the dialog, either accepting
   * or rejecting it, with the optional prompt filled.
   *
   * @param {Object}
   *        - {Boolean} accept: For "confirm", "prompt", "beforeunload" dialogs
   *          true will accept the dialog, false will cancel it. For "alert"
   *          dialogs, true or false closes the dialog in the same way.
   *        - {String} promptText: for "prompt" dialogs, used to fill the prompt
   *          input.
   */
  async handleJavaScriptDialog({ accept, promptText }) {
    if (!this.enabled) {
      throw new Error("Page domain is not enabled");
    }
    await this._dialogHandler.handleJavaScriptDialog({ accept, promptText });
  }

  /**
   * Emit the proper CDP event javascriptDialogOpening when a javascript dialog
   * opens for the current target.
   */
  _onDialogLoaded(e, data) {
    const { message, type } = data;
    // XXX: We rely on the tabmodal-dialog-loaded event (see DialogHandler.jsm)
    // which is inconsistent with the name "javascriptDialogOpening".
    // For correctness we should rely on an event fired _before_ the prompt is
    // visible, such as DOMWillOpenModalDialog. However the payload of this
    // event does not contain enough data to populate javascriptDialogOpening.
    //
    // Since the event is fired asynchronously, this should not have an impact
    // on the actual tests relying on this API.
    this.emit("Page.javascriptDialogOpening", { message, type });
  }
}
