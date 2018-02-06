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
    ]);
  }

  constructor() {
    super();

    this.amount = null;
    this._amount = document.createElement("currency-amount");
    this._amount.classList.add("amount");
    this._label = document.createElement("span");
    this._label.classList.add("label");
  }

  connectedCallback() {
    this.appendChild(this._amount);
    this.appendChild(this._label);
    super.connectedCallback();
  }

  render() {
    // An amount is required to render the shipping option.
    if (!this.amount) {
      return;
    }

    this._amount.currency = this.amount.currency;
    this._amount.value = this.amount.value;
    this._label.textContent = this.label;
  }
}

customElements.define("shipping-option", ShippingOption);
