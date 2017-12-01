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
    let owner = RichOption._createElement(fragment, "owner");
    let number = RichOption._createElement(fragment, "number");
    let expiration = RichOption._createElement(fragment, "expiration");
    let type = RichOption._createElement(fragment, "type");
    this.appendChild(fragment);

    this.elementMap = {
      owner,
      number,
      expiration,
      type,
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

    this.elementMap.owner.textContent = this.owner;
    this.elementMap.number.textContent = this.number;
    this.elementMap.expiration.textContent = this.expiration;
    this.elementMap.type.textContent = this.type;
  }
}

customElements.define("basic-card-option", BasicCardOption);
