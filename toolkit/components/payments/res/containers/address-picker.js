/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global PaymentStateSubscriberMixin, PaymentRequest */

"use strict";

/**
 * <address-picker></address-picker>
 * Container around <rich-select> (eventually providing add/edit links) with
 * <address-option> listening to savedAddresses.
 */

class AddressPicker extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();
    this.dropdown = document.createElement("rich-select");
  }

  connectedCallback() {
    this.appendChild(this.dropdown);
    super.connectedCallback();
  }

  render(state) {
    let {savedAddresses} = state;
    let desiredOptions = [];
    for (let [guid, address] of Object.entries(savedAddresses)) {
      let optionEl = this.dropdown.namedItem(guid);
      if (!optionEl) {
        optionEl = document.createElement("address-option");
        optionEl.name = guid;
        optionEl.guid = guid;
      }
      for (let [key, val] of Object.entries(address)) {
        optionEl.setAttribute(key, val);
      }
      desiredOptions.push(optionEl);
    }
    let el = null;
    while ((el = this.dropdown.popupBox.querySelector(":scope > address-option"))) {
      el.remove();
    }
    this.dropdown.popupBox.append(...desiredOptions);
  }
}

customElements.define("address-picker", AddressPicker);
