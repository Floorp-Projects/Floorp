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
  requestId: null,

  init() {
    // listen to content
    window.addEventListener("paymentChromeToContent", this);

    // listen to user events
    window.addEventListener("DOMContentLoaded", this, {once: true});

    // This scope is now ready to listen to the initialization data
    this.sendMessageToChrome("initializeRequest");
  },

  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded": {
        this.onPaymentRequestLoad();
        break;
      }
      case "click": {
        switch (event.target.id) {
          case "cancel": {
            this.onCancel();
            break;
          }
        }
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
        requestId: this.requestId,
        messageType,
      }, detail),
    });
    document.dispatchEvent(event);
  },

  onChromeToContent({detail}) {
    let {messageType, requestId} = detail;

    switch (messageType) {
      case "showPaymentRequest": {
        this.requestId = requestId;
        break;
      }
    }
  },

  onPaymentRequestLoad(requestId) {
    let cancelBtn = document.getElementById("cancel");
    cancelBtn.addEventListener("click", this, {once: true});

    window.addEventListener("unload", this, {once: true});
    this.sendMessageToChrome("paymentDialogReady");
  },

  onCancel() {
    this.sendMessageToChrome("paymentCancel");
  },

  onPaymentRequestUnload() {
    // remove listeners that may be used multiple times here
    window.removeEventListener("paymentChromeToContent", this);
  },
};

PaymentRequest.init();
