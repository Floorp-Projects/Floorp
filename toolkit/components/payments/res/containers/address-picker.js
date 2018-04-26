/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import AddressOption from "../components/address-option.js";
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import RichSelect from "../components/rich-select.js";

/**
 * <address-picker></address-picker>
 * Container around <rich-select> (eventually providing add/edit links) with
 * <address-option> listening to savedAddresses.
 */

export default class AddressPicker extends PaymentStateSubscriberMixin(HTMLElement) {
  static get observedAttributes() {
    return ["address-fields"];
  }

  constructor() {
    super();
    this.dropdown = new RichSelect();
    this.dropdown.addEventListener("change", this);
  }

  connectedCallback() {
    this.appendChild(this.dropdown);
    super.connectedCallback();
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (oldValue !== newValue) {
      this.render(this.requestStore.getState());
    }
  }

  /**
   * De-dupe and filter addresses for the given set of fields that will be visible
   *
   * @param {object} addresses
   * @param {array?} fieldNames - optional list of field names that be used when
   *                              de-duping and excluding entries
   * @returns {object} filtered copy of given addresses
   */
  filterAddresses(addresses, fieldNames = [
    "address-level1",
    "address-level2",
    "country",
    "name",
    "postal-code",
    "street-address",
  ]) {
    let uniques = new Set();
    let result = {};
    for (let [guid, address] of Object.entries(addresses)) {
      let addressCopy = {};
      let isMatch = false;
      // exclude addresses that are missing all of the requested fields
      for (let name of fieldNames) {
        if (address[name]) {
          isMatch = true;
          addressCopy[name] = address[name];
        }
      }
      if (isMatch) {
        let key = JSON.stringify(addressCopy);
        // exclude duplicated addresses
        if (!uniques.has(key)) {
          uniques.add(key);
          result[guid] = address;
        }
      }
    }
    return result;
  }

  render(state) {
    let {savedAddresses} = state;
    let desiredOptions = [];
    let fieldNames;
    if (this.hasAttribute("address-fields")) {
      let names = this.getAttribute("address-fields").split(/\s+/);
      if (names.length) {
        fieldNames = names;
      }
    }
    let filteredAddresses = this.filterAddresses(savedAddresses, fieldNames);

    for (let [guid, address] of Object.entries(filteredAddresses)) {
      let optionEl = this.dropdown.getOptionByValue(guid);
      if (!optionEl) {
        optionEl = new AddressOption();
        optionEl.value = guid;
      }

      for (let key of AddressOption.recordAttributes) {
        let val = address[key];
        if (val) {
          optionEl.setAttribute(key, val);
        } else {
          optionEl.removeAttribute(key);
        }
      }

      desiredOptions.push(optionEl);
    }

    let el = null;
    while ((el = this.dropdown.popupBox.querySelector(":scope > address-option"))) {
      el.remove();
    }
    for (let option of desiredOptions) {
      this.dropdown.popupBox.appendChild(option);
    }

    // Update selectedness after the options are updated
    let selectedAddressGUID = state[this.selectedStateKey];
    let optionWithGUID = this.dropdown.getOptionByValue(selectedAddressGUID);
    this.dropdown.selectedOption = optionWithGUID;

    if (selectedAddressGUID && !optionWithGUID) {
      throw new Error(`${this.selectedStateKey} option ${selectedAddressGUID}` +
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
    }
  }

  onChange(event) {
    let select = event.target;
    let selectedKey = this.selectedStateKey;
    if (selectedKey) {
      this.requestStore.setState({
        [selectedKey]: select.selectedOption && select.selectedOption.guid,
      });
    }
  }
}

customElements.define("address-picker", AddressPicker);
