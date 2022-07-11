/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPrompter"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPrompter");

class GeckoViewPrompter {
  constructor(aParent) {
    this.id = Services.uuid
      .generateUUID()
      .toString()
      .slice(1, -1); // Discard surrounding braces

    if (aParent) {
      if (Window.isInstance(aParent)) {
        this._domWin = aParent;
      } else if (aParent.window) {
        this._domWin = aParent.window;
      } else {
        this._domWin =
          aParent.embedderElement && aParent.embedderElement.ownerGlobal;
      }
    }

    if (!this._domWin) {
      this._domWin = Services.wm.getMostRecentWindow("navigator:geckoview");
    }

    this._innerWindowId = this._domWin?.browsingContext.currentWindowContext.innerWindowId;
  }

  get domWin() {
    return this._domWin;
  }

  get prompterActor() {
    const actor = this.domWin?.windowGlobalChild.getActor("GeckoViewPrompter");
    return actor;
  }

  _changeModalState(aEntering) {
    if (!this._domWin) {
      // Allow not having a DOM window.
      return true;
    }
    // Accessing the document object can throw if this window no longer exists. See bug 789888.
    try {
      const winUtils = this._domWin.windowUtils;
      if (!aEntering) {
        winUtils.leaveModalState();
      }

      const event = this._domWin.document.createEvent("Events");
      event.initEvent(
        aEntering ? "DOMWillOpenModalDialog" : "DOMModalDialogClosed",
        true,
        true
      );
      winUtils.dispatchEventToChromeOnly(this._domWin, event);

      if (aEntering) {
        winUtils.enterModalState();
      }
      return true;
    } catch (ex) {
      Cu.reportError("Failed to change modal state: " + ex);
    }
    return false;
  }

  _dismissUi() {
    this.prompterActor?.dismissPrompt(this);
  }

  accept(aInputText = this.inputText) {
    if (this.callback) {
      let acceptMsg = {};
      switch (this.message.type) {
        case "alert":
          acceptMsg = null;
          break;
        case "button":
          acceptMsg.button = 0;
          break;
        case "text":
          acceptMsg.text = aInputText;
          break;
        default:
          acceptMsg = null;
          break;
      }
      this.callback(acceptMsg);
      // Notify the UI that this prompt should be hidden.
      this._dismissUi();
    }
  }

  dismiss() {
    this.callback(null);
    // Notify the UI that this prompt should be hidden.
    this._dismissUi();
  }

  getPromptType() {
    switch (this.message.type) {
      case "alert":
        return this.message.checkValue ? "alertCheck" : "alert";
      case "button":
        return this.message.checkValue ? "confirmCheck" : "confirm";
      case "text":
        return this.message.checkValue ? "promptCheck" : "prompt";
      default:
        return this.message.type;
    }
  }

  getPromptText() {
    return this.message.msg;
  }

  getInputText() {
    return this.inputText;
  }

  setInputText(aInput) {
    this.inputText = aInput;
  }

  /**
   * Shows a native prompt, and then spins the event loop for this thread while we wait
   * for a response
   */
  showPrompt(aMsg) {
    let result = undefined;
    if (!this._domWin || !this._changeModalState(/* aEntering */ true)) {
      return result;
    }
    try {
      this.asyncShowPrompt(aMsg, res => (result = res));

      // Spin this thread while we wait for a result
      Services.tm.spinEventLoopUntil(
        "GeckoViewPrompter.jsm:showPrompt",
        () => this._domWin.closed || result !== undefined
      );
    } finally {
      this._changeModalState(/* aEntering */ false);
    }
    return result;
  }

  checkInnerWindow() {
    // Checks that the innerWindow where this prompt was created still matches
    // the current innerWindow.
    // This checks will fail if the page navigates away, making this prompt
    // obsolete.
    return (
      this._innerWindowId ===
      this._domWin.browsingContext.currentWindowContext.innerWindowId
    );
  }

  asyncShowPromptPromise(aMsg) {
    return new Promise(resolve => {
      this.asyncShowPrompt(aMsg, resolve);
    });
  }

  async asyncShowPrompt(aMsg, aCallback) {
    this.message = aMsg;
    this.inputText = aMsg.value;
    this.callback = aCallback;

    aMsg.id = this.id;

    let response = null;
    try {
      if (this.checkInnerWindow()) {
        response = await this.prompterActor.prompt(this, aMsg);
      }
    } catch (error) {
      // Nothing we can do really, we will treat this as a dismiss.
      warn`Error while prompting: ${error}`;
    }

    if (!this.checkInnerWindow()) {
      // Page has navigated away, let's dismiss the prompt
      aCallback(null);
    } else {
      aCallback(response);
    }
    // This callback object is tied to the Java garbage collector because
    // it is invoked from Java. Manually release the target callback
    // here; otherwise we may hold onto resources for too long, because
    // we would be relying on both the Java and the JS garbage collectors
    // to run.
    aMsg = undefined;
    aCallback = undefined;
  }

  update(aMsg) {
    this.message = aMsg;
    aMsg.id = this.id;
    this.prompterActor?.updatePrompt(aMsg);
  }
}
