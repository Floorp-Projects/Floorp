/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPrompter"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPrompter");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gUUIDGenerator",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

class GeckoViewPrompter {
  constructor(aParent) {
    this.id = gUUIDGenerator
      .generateUUID()
      .toString()
      .slice(1, -1); // Discard surrounding braces

    if (aParent) {
      if (aParent instanceof Window) {
        this._domWin = aParent;
      } else if (aParent.window) {
        this._domWin = aParent.window;
      } else {
        this._domWin =
          aParent.embedderElement && aParent.embedderElement.ownerGlobal;
      }
    }

    if (this._domWin) {
      this._dispatcher = GeckoViewUtils.getDispatcherForWindow(this._domWin);
    }

    if (!this._dispatcher) {
      [
        this._dispatcher,
        this._domWin,
      ] = GeckoViewUtils.getActiveDispatcherAndWindow();
    }

    this._innerWindowId = this._domWin?.browsingContext.currentWindowContext.innerWindowId;
  }

  get domWin() {
    return this._domWin;
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

  dismiss() {
    this._dispatcher.dispatch("GeckoView:Prompt:Dismiss", { id: this.id });
  }

  asyncShowPrompt(aMsg, aCallback) {
    let handled = false;
    const onResponse = response => {
      if (handled) {
        return;
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
      handled = true;
    };

    if (!this._dispatcher || !this.checkInnerWindow()) {
      onResponse(null);
      return;
    }

    aMsg.id = this.id;
    this._dispatcher.dispatch("GeckoView:Prompt", aMsg, {
      onSuccess: onResponse,
      onError: error => {
        Cu.reportError("Prompt error: " + error);
        onResponse(null);
      },
    });
  }
}
