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
      "number",
      "owner",
      "type",
    ]);
  }

  connectedCallback() {
    for (let child of this.children) {
      child.remove();
    }

    let fragment = document.createDocumentFragment();
    RichOption._createElement(fragment, "owner");
    RichOption._createElement(fragment, "number");
    RichOption._createElement(fragment, "expiration");
    RichOption._createElement(fragment, "type");
    this.appendChild(fragment);

    super.connectedCallback();
  }

  render() {
    if (!this.parentNode) {
      return;
    }

    this.querySelector(".owner").textContent = this.owner;
    this.querySelector(".number").textContent = this.number;
    this.querySelector(".expiration").textContent = this.expiration;
    this.querySelector(".type").textContent = this.type;
  }
}

customElements.define("basic-card-option", BasicCardOption);
