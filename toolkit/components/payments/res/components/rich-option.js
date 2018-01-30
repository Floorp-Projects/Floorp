/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * <rich-select>
 *  <rich-option></rich-option>
 * </rich-select>
 */

/* global ObservedPropertiesMixin */
/* exported RichOption */

class RichOption extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "selected",
      "value",
    ];
  }

  connectedCallback() {
    this.classList.add("rich-option");
    this.render();
    this.addEventListener("click", this);
    this.addEventListener("keydown", this);
  }

  render() {}

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        this.onClick(event);
        break;
      }
      case "keydown": {
        this.onKeyDown(event);
        break;
      }
    }
  }

  onClick(event) {
    if (this.closest("rich-select").open &&
        !this.disabled &&
        event.button == 0) {
      for (let option of this.parentNode.children) {
        option.selected = option == this;
      }
    }
  }

  onKeyDown(event) {
    if (!this.disabled &&
        event.which == 13 /* Enter */) {
      for (let option of this.parentNode.children) {
        option.selected = option == this;
      }
    }
  }

  static _createElement(fragment, className) {
    let element = document.createElement("span");
    element.classList.add(className);
    fragment.appendChild(element);
    return element;
  }
}

customElements.define("rich-option", RichOption);
