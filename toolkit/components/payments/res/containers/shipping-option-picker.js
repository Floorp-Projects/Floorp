/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global PaymentStateSubscriberMixin */

"use strict";

/**
 * <shipping-option-picker></shipping-option-picker>
 * Container around <rich-select> with
 * <rich-option> listening to shippingOptions.
 */

class ShippingOptionPicker extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();
    this.dropdown = document.createElement("rich-select");
    this.dropdown.addEventListener("change", this);
  }

  connectedCallback() {
    this.appendChild(this.dropdown);
    super.connectedCallback();
  }

  render(state) {
    let {shippingOptions} = state.request.paymentDetails;
    let desiredOptions = [];
    for (let option of shippingOptions) {
      let optionEl = this.dropdown.getOptionByValue(option.id);
      if (!optionEl) {
        optionEl = document.createElement("shipping-option");
        optionEl.value = option.id;
      }
      optionEl.label = option.label;
      optionEl.amountCurrency = option.amount.currency;
      optionEl.amountValue = option.amount.value;
      desiredOptions.push(optionEl);
    }
    let el = null;
    while ((el = this.dropdown.popupBox.querySelector(":scope > shipping-option"))) {
      el.remove();
    }
    for (let option of desiredOptions) {
      this.dropdown.popupBox.appendChild(option);
    }

    // Update selectedness after the options are updated
    let selectedShippingOption = state.selectedShippingOption;
    let selectedOptionEl =
      this.dropdown.getOptionByValue(selectedShippingOption);
    this.dropdown.selectedOption = selectedOptionEl;

    if (selectedShippingOption && !selectedOptionEl) {
      throw new Error(`Selected shipping option ${selectedShippingOption} ` +
                      `does not exist in option elements`);
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "change": {
        this.onChange(event);
        break;
      }
    }
  }

  onChange(event) {
    let select = event.target;
    let selectedOptionId = select.selectedOption && select.selectedOption.value;
    this.requestStore.setState({
      selectedShippingOption: selectedOptionId,
    });
  }
}

customElements.define("shipping-option-picker", ShippingOptionPicker);
