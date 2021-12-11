/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  class MozCheckbox extends MozElements.BaseText {
    static get markup() {
      return `
      <image class="checkbox-check"/>
      <hbox class="checkbox-label-box" flex="1">
        <image class="checkbox-icon"/>
        <label class="checkbox-label" flex="1"/>
      </hbox>
      `;
    }

    constructor() {
      super();

      // While it would seem we could do this by handling oncommand, we need can't
      // because any external oncommand handlers might get called before ours, and
      // then they would see the incorrect value of checked.
      this.addEventListener("click", event => {
        if (event.button === 0 && !this.disabled) {
          this.checked = !this.checked;
        }
      });
      this.addEventListener("keypress", event => {
        if (event.key == " ") {
          this.checked = !this.checked;
          // Prevent page from scrolling on the space key.
          event.preventDefault();
        }
      });
    }

    static get inheritedAttributes() {
      return {
        ".checkbox-check": "disabled,checked,native",
        ".checkbox-label": "text=label,accesskey,native",
        ".checkbox-icon": "src,native",
      };
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      this.textContent = "";
      this.appendChild(this.constructor.fragment);

      this.initializeAttributeInheritance();
    }

    set checked(val) {
      let change = val != (this.getAttribute("checked") == "true");
      if (val) {
        this.setAttribute("checked", "true");
      } else {
        this.removeAttribute("checked");
      }

      if (change) {
        let event = document.createEvent("Events");
        event.initEvent("CheckboxStateChange", true, true);
        this.dispatchEvent(event);
      }
    }

    get checked() {
      return this.getAttribute("checked") == "true";
    }
  }

  MozCheckbox.contentFragment = null;

  customElements.define("checkbox", MozCheckbox);
}
