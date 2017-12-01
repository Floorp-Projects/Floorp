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
    let recipient = RichOption._createElement(fragment, "recipient");
    let addressLine = RichOption._createElement(fragment, "addressLine");
    let email = RichOption._createElement(fragment, "email");
    let phone = RichOption._createElement(fragment, "phone");
    this.appendChild(fragment);

    this.elementMap = {
      recipient,
      addressLine,
      email,
      phone,
    };

    super.connectedCallback();
  }

  disconnectedCallback() {
    this.elementMap = {};
  }

  render() {
    if (!this.parentNode) {
      return;
    }

    this.elementMap.recipient.textContent = this.recipient;
    this.elementMap.addressLine.textContent =
      `${this.addressLine} ${this.city} ${this.region} ${this.postalCode} ${this.country}`;
    this.elementMap.email.textContent = this.email;
    this.elementMap.phone.textContent = this.phone;
  }
}

customElements.define("address-option", AddressOption);

