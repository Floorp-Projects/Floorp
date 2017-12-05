/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * <currency-amount value="7.5" currency="USD"></currency-amount>
 */

/* global ObservedPropertiesMixin */

class CurrencyAmount extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return ["currency", "value"];
  }

  render() {
    let output = "";
    try {
      if (this.value && this.currency) {
        let number = Number.parseFloat(this.value);
        if (Number.isNaN(number) || !Number.isFinite(number)) {
          throw new RangeError("currency-amount value must be a finite number");
        }
        const formatter = new Intl.NumberFormat(navigator.languages, {
          style: "currency",
          currency: this.currency,
          currencyDisplay: "symbol",
        });
        output = formatter.format(this.value);
      }
    } finally {
      this.textContent = output;
    }
  }
}

customElements.define("currency-amount", CurrencyAmount);
