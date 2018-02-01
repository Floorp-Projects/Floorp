/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * <ul>
 *  <payment-details-item
                          label="Some item"
                          amount-value="1.00"
                          amount-currency="USD"></payment-details-item>
 * </ul>
 */

/* global ObservedPropertiesMixin */

class PaymentDetailsItem extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "label",
      "amount-currency",
      "amount-value",
    ];
  }

  constructor() {
    super();
    this._label = document.createElement("span");
    this._label.classList.add("label");
    this._currencyAmount = document.createElement("currency-amount");
  }

  connectedCallback() {
    this.appendChild(this._label);
    this.appendChild(this._currencyAmount);

    if (super.connectedCallback) {
      super.connectedCallback();
    }
  }

  render() {
    this._currencyAmount.value = this.amountValue;
    this._currencyAmount.currency = this.amountCurrency;
    this._label.textContent = this.label;
  }
}

customElements.define("payment-details-item", PaymentDetailsItem);

