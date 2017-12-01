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
  static get observedAttributes() { return ["selected", "hidden"]; }

  constructor() {
    super();

    this.addEventListener("click", this);
    this.addEventListener("keypress", this);
  }

  connectedCallback() {
    this.render();
    let richSelect = this.closest("rich-select");
    if (richSelect && richSelect.render) {
      richSelect.render();
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        this.onClick(event);
        break;
      }
      case "keypress": {
        this.onKeyPress(event);
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

  onKeyPress(event) {
    if (!this.disabled &&
        event.which == 13 /* Enter */) {
      for (let option of this.parentNode.children) {
        option.selected = option == this;
      }
    }
  }

  get selected() {
    return this.hasAttribute("selected");
  }

  set selected(value) {
    if (value) {
      let oldSelectedOptions = this.parentNode.querySelectorAll("[selected]");
      for (let option of oldSelectedOptions) {
        option.removeAttribute("selected");
      }
      this.setAttribute("selected", value);
    } else {
      this.removeAttribute("selected");
    }
    let richSelect = this.closest("rich-select");
    if (richSelect && richSelect.render) {
      richSelect.render();
    }
    return value;
  }

  static _createElement(fragment, className) {
    let element = document.createElement("span");
    element.classList.add(className);
    fragment.appendChild(element);
    return element;
  }
}
