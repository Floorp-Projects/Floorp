/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Loaded in the unprivileged frame of each payment dialog.
 *
 * Communicates with privileged code via DOM Events.
 */

"use strict";

let PaymentRequest = {
  domReadyPromise: null,

  init() {
    // listen to content
    window.addEventListener("paymentChromeToContent", this);

    window.addEventListener("keypress", this);

    this.domReadyPromise = new Promise(function dcl(resolve) {
      window.addEventListener("DOMContentLoaded", resolve, {once: true});
    }).then(this.handleEvent.bind(this));

    // This scope is now ready to listen to the initialization data
    this.sendMessageToChrome("initializeRequest");
  },

  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded": {
        this.onPaymentRequestLoad();
        break;
      }
      case "keypress": {
        if (event.code != "KeyD" || !event.altKey || !event.ctrlKey) {
          break;
        }
        let debuggingConsole = document.getElementById("debugging-console");
        debuggingConsole.hidden = !debuggingConsole.hidden;
        break;
      }
      case "unload": {
        this.onPaymentRequestUnload();
        break;
      }
      case "paymentChromeToContent": {
        this.onChromeToContent(event);
        break;
      }
      default: {
        throw new Error("Unexpected event type");
      }
    }
  },

  sendMessageToChrome(messageType, detail = {}) {
    let event = new CustomEvent("paymentContentToChrome", {
      bubbles: true,
      detail: Object.assign({
        messageType,
      }, detail),
    });
    document.dispatchEvent(event);
  },

  onChromeToContent({detail}) {
    let {messageType} = detail;

    switch (messageType) {
      case "showPaymentRequest": {
        this.onShowPaymentRequest(detail);
        break;
      }
      case "updateState": {
        document.querySelector("payment-dialog").setStateFromParent(detail);
        break;
      }
    }
  },

  onPaymentRequestLoad(requestId) {
    window.addEventListener("unload", this, {once: true});
    this.sendMessageToChrome("paymentDialogReady");
  },

  async onShowPaymentRequest(detail) {
    // Handle getting called before the DOM is ready.
    await this.domReadyPromise;

    document.querySelector("payment-dialog").setStateFromParent({
      request: detail.request,
      savedAddresses: detail.savedAddresses,
      savedBasicCards: detail.savedBasicCards,
    });
  },

  cancel() {
    this.sendMessageToChrome("paymentCancel");
  },

  pay(data) {
    this.sendMessageToChrome("pay", data);
  },

  onPaymentRequestUnload() {
    // remove listeners that may be used multiple times here
    window.removeEventListener("paymentChromeToContent", this);
  },
};

PaymentRequest.init();
