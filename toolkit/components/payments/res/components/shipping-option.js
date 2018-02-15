/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * <rich-select>
 *  <shipping-option></shipping-option>
 * </rich-select>
 */

/* global ObservedPropertiesMixin, RichOption */

class ShippingOption extends ObservedPropertiesMixin(RichOption) {
  static get observedAttributes() {
    return RichOption.observedAttributes.concat([
      "label",
      "amount-currency",
      "amount-value",
    ]);
  }

  constructor() {
    super();

    this.amount = null;
    this._currencyAmount = document.createElement("currency-amount");
    this._currencyAmount.classList.add("amount");
    this._label = document.createElement("span");
    this._label.classList.add("label");
  }

  connectedCallback() {
    this.appendChild(this._currencyAmount);
    this.appendChild(this._label);
    super.connectedCallback();
  }

  render() {
    this._label.textContent = this.label;
    this._currencyAmount.currency = this.amountCurrency;
    this._currencyAmount.value = this.amountValue;
    // Need to call render after setting these properties
    // if we want the amount to get displayed in the same
    // render pass as the label.
    this._currencyAmount.render();
  }
}

customElements.define("shipping-option", ShippingOption);
