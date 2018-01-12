/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * <rich-select>
 *  <address-option addressLine="1234 Anywhere St"
 *                  city="Some City"
 *                  country="USA"
 *                  dependentLocality=""
 *                  languageCode="en-US"
 *                  phone=""
 *                  postalCode="90210"
 *                  recipient="Jared Wein"
 *                  region="MI"></address-option>
 * </rich-select>
 */

/* global ObservedPropertiesMixin, RichOption */

class AddressOption extends ObservedPropertiesMixin(RichOption) {
  static get observedAttributes() {
    return RichOption.observedAttributes.concat([
      "addressLine",
      "city",
      "country",
      "dependentLocality",
      "email",
      "languageCode",
      "organization",
      "phone",
      "postalCode",
      "recipient",
      "region",
      "sortingCode",
    ]);
  }

  connectedCallback() {
    for (let child of this.children) {
      child.remove();
    }

    let fragment = document.createDocumentFragment();
    RichOption._createElement(fragment, "recipient");
    RichOption._createElement(fragment, "addressLine");
    RichOption._createElement(fragment, "email");
    RichOption._createElement(fragment, "phone");
    this.appendChild(fragment);

    super.connectedCallback();
  }

  render() {
    if (!this.parentNode) {
      return;
    }

    this.querySelector(".recipient").textContent = this.recipient;
    this.querySelector(".addressLine").textContent =
      `${this.addressLine} ${this.city} ${this.region} ${this.postalCode} ${this.country}`;
    this.querySelector(".email").textContent = this.email;
    this.querySelector(".phone").textContent = this.phone;
  }
}

customElements.define("address-option", AddressOption);

