/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * <rich-select>
 *  <basic-card-option></basic-card-option>
 * </rich-select>
 */

/* global ObservedPropertiesMixin, RichOption */

class BasicCardOption extends ObservedPropertiesMixin(RichOption) {
  static get observedAttributes() {
    return RichOption.observedAttributes.concat([
      "expiration",
      "guid",
      "number",
      "owner",
      "type",
    ]);
  }

  constructor() {
    super();

    for (let name of ["owner", "number", "expiration", "type"]) {
      this[`_${name}`] = document.createElement("span");
      this[`_${name}`].classList.add(name);
    }
  }

  connectedCallback() {
    for (let name of ["owner", "number", "expiration", "type"]) {
      this.appendChild(this[`_${name}`]);
    }
    super.connectedCallback();
  }

  render() {
    this._owner.textContent = this.owner;
    this._number.textContent = this.number;
    this._expiration.textContent = this.expiration;
    this._type.textContent = this.type;
  }
}

customElements.define("basic-card-option", BasicCardOption);
