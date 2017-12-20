/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global PaymentStateSubscriberMixin, PaymentRequest */

"use strict";

/**
 * <payment-dialog></payment-dialog>
 */

class PaymentDialog extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();
    this._template = document.getElementById("payment-dialog-template");
  }

  connectedCallback() {
    let contents = document.importNode(this._template.content, true);
    this._hostNameEl = contents.querySelector("#host-name");

    this._cancelButton = contents.querySelector("#cancel");
    this._cancelButton.addEventListener("click", this.cancelRequest);

    this.appendChild(contents);

    super.connectedCallback();
  }

  disconnectedCallback() {
    this._cancelButtonEl.removeEventListener("click", this.cancelRequest);
    super.disconnectedCallback();
  }

  cancelRequest() {
    PaymentRequest.cancel();
  }

  setLoadingState(state) {
    this.requestStore.setState(state);
  }

  render(state) {
    let request = state.request;
    this._hostNameEl.textContent = request.topLevelPrincipal.URI.displayHost;

    let totalItem = request.paymentDetails.totalItem;
    let totalAmountEl = this.querySelector("#total > currency-amount");
    totalAmountEl.value = totalItem.amount.value;
    totalAmountEl.currency = totalItem.amount.currency;
  }
}

customElements.define("payment-dialog", PaymentDialog);
