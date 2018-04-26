/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import BasicCardOption from "../components/basic-card-option.js";
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import RichSelect from "../components/rich-select.js";
import paymentRequest from "../paymentRequest.js";

/**
 * <payment-method-picker></payment-method-picker>
 * Container around add/edit links and <rich-select> with
 * <basic-card-option> listening to savedBasicCards.
 */

export default class PaymentMethodPicker extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();
    this.dropdown = new RichSelect();
    this.dropdown.addEventListener("change", this);
    this.spacerText = document.createTextNode(" ");
    this.securityCodeInput = document.createElement("input");
    this.securityCodeInput.autocomplete = "off";
    this.securityCodeInput.size = 3;
    this.securityCodeInput.addEventListener("change", this);
    this.addLink = document.createElement("a");
    this.addLink.href = "javascript:void(0)";
    this.addLink.textContent = this.dataset.addLinkLabel;
    this.addLink.addEventListener("click", this);
    this.editLink = document.createElement("a");
    this.editLink.href = "javascript:void(0)";
    this.editLink.textContent = this.dataset.editLinkLabel;
    this.editLink.addEventListener("click", this);
  }

  connectedCallback() {
    this.appendChild(this.dropdown);
    this.appendChild(this.spacerText);
    this.appendChild(this.securityCodeInput);
    this.appendChild(this.addLink);
    this.append(" ");
    this.appendChild(this.editLink);
    super.connectedCallback();
  }

  render(state) {
    let basicCards = paymentRequest.getBasicCards(state);
    let desiredOptions = [];
    for (let [guid, basicCard] of Object.entries(basicCards)) {
      let optionEl = this.dropdown.getOptionByValue(guid);
      if (!optionEl) {
        optionEl = new BasicCardOption();
        optionEl.value = guid;
      }
      for (let key of BasicCardOption.recordAttributes) {
        let val = basicCard[key];
        if (val) {
          optionEl.setAttribute(key, val);
        } else {
          optionEl.removeAttribute(key);
        }
      }
      desiredOptions.push(optionEl);
    }
    let el = null;
    while ((el = this.dropdown.popupBox.querySelector(":scope > basic-card-option"))) {
      el.remove();
    }
    for (let option of desiredOptions) {
      this.dropdown.popupBox.appendChild(option);
    }

    // Update selectedness after the options are updated
    let selectedPaymentCardGUID = state[this.selectedStateKey];
    let optionWithGUID = this.dropdown.getOptionByValue(selectedPaymentCardGUID);
    this.dropdown.selectedOption = optionWithGUID;

    if (selectedPaymentCardGUID && !optionWithGUID) {
      throw new Error(`${this.selectedStateKey} option ${selectedPaymentCardGUID}` +
                      `does not exist in options`);
    }
  }

  get selectedStateKey() {
    return this.getAttribute("selected-state-key");
  }

  handleEvent(event) {
    switch (event.type) {
      case "change": {
        this.onChange(event);
        break;
      }
      case "click": {
        this.onClick(event);
        break;
      }
    }
  }

  onChange({target}) {
    let selectedKey = this.selectedStateKey;
    let stateChange = {};

    if (!selectedKey) {
      return;
    }

    switch (target) {
      case this.dropdown: {
        stateChange[selectedKey] = target.selectedOption && target.selectedOption.guid;
        // Select the security code text since the user is likely to edit it next.
        // We don't want to do this if the user simply blurs the dropdown.
        this.securityCodeInput.select();
        break;
      }
      case this.securityCodeInput: {
        stateChange[selectedKey + "SecurityCode"] = this.securityCodeInput.value;
        break;
      }
      default: {
        return;
      }
    }

    this.requestStore.setState(stateChange);
  }

  onClick({target}) {
    let nextState = {
      page: {
        id: "basic-card-page",
      },
    };

    switch (target) {
      case this.addLink: {
        nextState.page.guid = null;
        break;
      }
      case this.editLink: {
        let state = this.requestStore.getState();
        let selectedPaymentCardGUID = state[this.selectedStateKey];
        nextState.page.guid = selectedPaymentCardGUID;
        break;
      }
      default: {
        throw new Error("Unexpected onClick");
      }
    }

    this.requestStore.setState(nextState);
  }
}

customElements.define("payment-method-picker", PaymentMethodPicker);
