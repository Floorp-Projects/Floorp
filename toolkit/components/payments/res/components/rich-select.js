/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ObservedPropertiesMixin */

/**
 * <rich-select>
 *  <rich-option></rich-option>
 * </rich-select>
 *
 * Note: The only supported way to change the selected option is via the
 *       `selectedOption` setter.
 */
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

    this._mutationObserver = new MutationObserver((mutations) => {
      for (let mutation of mutations) {
        for (let addedNode of mutation.addedNodes) {
          if (addedNode.nodeType != Node.ELEMENT_NODE ||
              !addedNode.matches(".rich-option:not(.rich-select-selected-clone)")) {
            continue;
          }
          // Move the added rich option to the popup.
          this.popupBox.appendChild(addedNode);
        }
      }
    });
    this._mutationObserver.observe(this, {
      childList: true,
    });
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


  /**
   * This is the only supported method of changing the selected option. Do not
   * manipulate the `selected` property or attribute on options directly.
   * @param {HTMLOptionElement} option
   */
  set selectedOption(option) {
    for (let child of this.popupBox.children) {
      child.selected = child == option;
    }

    this.render();
  }

  getOptionByValue(value) {
    return this.popupBox.querySelector(`:scope > [value="${CSS.escape(value)}"]`);
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
      if (aAttr == "selected") {
        continue;
      }
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

    let options = this.querySelectorAll(":scope > .rich-option:not(.rich-select-selected-clone)");
    for (let option of options) {
      popupBox.appendChild(option);
    }

    let selectedChild;
    for (let child of popupBox.children) {
      if (child.selected) {
        selectedChild = child;
        break;
      }
    }

    let selectedClone = this.querySelector(":scope > .rich-select-selected-clone");
    if (!this._optionsAreEquivalent(selectedClone, selectedChild)) {
      if (selectedClone) {
        selectedClone.remove();
      }

      if (selectedChild) {
        selectedClone = selectedChild.cloneNode(false);
        selectedClone.removeAttribute("id");
        selectedClone.removeAttribute("selected");
      } else {
        selectedClone = document.createElement("rich-option");
        selectedClone.textContent = "(None selected)";
      }
      selectedClone.classList.add("rich-select-selected-clone");
      selectedClone = this.appendChild(selectedClone);
    }
  }
}

customElements.define("rich-select", RichSelect);
