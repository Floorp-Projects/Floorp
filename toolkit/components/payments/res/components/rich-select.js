/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * <rich-select>
 *  <rich-option></rich-option>
 * </rich-select>
 */

/* global ObservedPropertiesMixin */

class RichSelect extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "open",
      "disabled",
      "hidden",
    ];
  }

  constructor() {
    super();

    this.addEventListener("blur", this);
    this.addEventListener("click", this);
    this.addEventListener("keydown", this);
  }

  connectedCallback() {
    this.setAttribute("tabindex", "0");
    this.render();
  }

  get popupBox() {
    return this.querySelector(":scope > .rich-select-popup-box");
  }

  get selectedOption() {
    return this.popupBox.querySelector(":scope > [selected]");
  }

  handleEvent(event) {
    switch (event.type) {
      case "blur": {
        this.onBlur(event);
        break;
      }
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

  onBlur(event) {
    if (event.target == this) {
      this.open = false;
    }
  }

  onClick(event) {
    if (!this.disabled &&
        event.button == 0) {
      this.open = !this.open;
    }
  }

  onKeyDown(event) {
    if (event.key == " ") {
      this.open = !this.open;
    } else if (event.key == "ArrowDown") {
      let selectedOption = this.selectedOption;
      let next = selectedOption.nextElementSibling;
      if (next) {
        next.selected = true;
        selectedOption.selected = false;
      }
    } else if (event.key == "ArrowUp") {
      let selectedOption = this.selectedOption;
      let next = selectedOption.previousElementSibling;
      if (next) {
        next.selected = true;
        selectedOption.selected = false;
      }
    } else if (event.key == "Enter" ||
               event.key == "Escape") {
      this.open = false;
    }
  }

  _optionsAreEquivalent(a, b) {
    if (!a || !b) {
      return false;
    }

    let aAttrs = a.constructor.observedAttributes;
    let bAttrs = b.constructor.observedAttributes;
    if (aAttrs.length != bAttrs.length) {
      return false;
    }

    for (let aAttr of aAttrs) {
      if (a.getAttribute(aAttr) != b.getAttribute(aAttr)) {
        return false;
      }
    }

    return true;
  }

  render() {
    let popupBox = this.popupBox;
    if (!popupBox) {
      popupBox = document.createElement("div");
      popupBox.classList.add("rich-select-popup-box");
      this.appendChild(popupBox);
    }

    /* eslint-disable max-len */
    let options =
      this.querySelectorAll(":scope > :not(.rich-select-popup-box):not(.rich-select-selected-clone)");
    /* eslint-enable max-len */
    for (let option of options) {
      popupBox.appendChild(option);
    }

    let selectedChild;
    for (let child of popupBox.children) {
      if (child.selected) {
        selectedChild = child;
      }
    }
    if (!selectedChild && popupBox.children.length) {
      selectedChild = popupBox.children[0];
      selectedChild.selected = true;
    }

    if (!this._optionsAreEquivalent(this._selectedChild, selectedChild)) {
      let selectedClone = this.querySelector(":scope > .rich-select-selected-clone");
      if (selectedClone) {
        selectedClone.remove();
      }

      if (selectedChild) {
        this._selectedChild = selectedChild;
        selectedClone = selectedChild.cloneNode(false);
        selectedClone.removeAttribute("id");
        selectedClone.classList.add("rich-select-selected-clone");
        selectedClone = this.appendChild(selectedClone);
      }
    }
  }
}

customElements.define("rich-select", RichSelect);
