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
  request: null,
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
      case "click": {
        switch (event.target.id) {
          case "cancel": {
            this.onCancel();
            break;
          }
        }
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
        this.request = detail.request;
        this.onShowPaymentRequest();
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

  async onShowPaymentRequest() {
    // Handle getting called before the DOM is ready.
    await this.domReadyPromise;

    let hostNameEl = document.getElementById("host-name");
    hostNameEl.textContent = this.request.topLevelPrincipal.URI.displayHost;

    let totalItem = this.request.paymentDetails.totalItem;
    let totalEl = document.getElementById("total");
    let currencyEl = totalEl.querySelector("currency-amount");
    currencyEl.value = totalItem.amount.value;
    currencyEl.currency = totalItem.amount.currency;
    totalEl.querySelector(".label").textContent = totalItem.label;
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
