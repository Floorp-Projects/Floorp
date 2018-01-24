/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * <rich-select>
 *  <address-option guid="98hgvnbmytfc"
 *                  address-level1="MI"
 *                  address-level2="Some City"
 *                  email="foo@example.com"
 *                  country="USA"
 *                  name="Jared Wein"
 *                  postal-code="90210"
 *                  street-address="1234 Anywhere St"
 *                  tel="+1 650 555-5555"></address-option>
 * </rich-select>
 *
 * Attribute names follow ProfileStorage.jsm.
 */

/* global ObservedPropertiesMixin, RichOption */

class AddressOption extends ObservedPropertiesMixin(RichOption) {
  static get observedAttributes() {
    return RichOption.observedAttributes.concat([
      "address-level1",
      "address-level2",
      "country",
      "email",
      "guid",
      "name",
      "postal-code",
      "street-address",
      "tel",
    ]);
  }

  connectedCallback() {
    for (let child of this.children) {
      child.remove();
    }

    let fragment = document.createDocumentFragment();
    RichOption._createElement(fragment, "name");
    RichOption._createElement(fragment, "street-address");
    RichOption._createElement(fragment, "email");
    RichOption._createElement(fragment, "tel");
    this.appendChild(fragment);

    super.connectedCallback();
  }

  render() {
    if (!this.parentNode) {
      return;
    }

    this.querySelector(".name").textContent = this.name;
    this.querySelector(".street-address").textContent = `${this.streetAddress} ` +
      `${this.addressLevel2} ${this.addressLevel1} ${this.postalCode} ${this.country}`;
    this.querySelector(".email").textContent = this.email;
    this.querySelector(".tel").textContent = this.tel;
  }
}

customElements.define("address-option", AddressOption);
