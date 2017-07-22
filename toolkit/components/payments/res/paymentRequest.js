/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let PaymentRequest = {
  requestId: null,

  init() {
    // listen to content
    window.addEventListener("paymentChromeToContent", this);

    // listen to user events
    window.addEventListener("load", this, {once: true});
  },

  handleEvent(event) {
    switch (event.type) {
      case "load": {
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
        this.onPaymentRequestLoad();
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

  sendMessageToContent(messageType, detail = {}) {
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
  },

  onCancel() {
    this.sendMessageToContent("paymentCancel");
  },

  onPaymentRequestUnload() {
    // remove listeners that may be used multiple times here
    window.removeEventListener("paymentChromeToContent", this);
  },
};

PaymentRequest.init();
