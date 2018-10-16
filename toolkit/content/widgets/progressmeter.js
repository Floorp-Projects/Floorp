/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{

/**
 * XUL progressmeter element.
 */
class MozProgressmeter extends MozXULElement {
  get mode() {
    return this.getAttribute("mode");
  }

  set mode(val) {
    if (this.mode != val) {
      this.setAttribute("mode", val);
    }
    return val;
  }

  get value() {
    return this.getAttribute("value") || "0";
  }

  set value(val) {
    let p = Math.round(val);
    let max = Math.round(this.max);
    if (p < 0) {
      p = 0;
    } else if (p > max) {
      p = max;
    }

    let c = this.value;
    if (p != c) {
      let delta = p - c;
      if (delta < 0) {
        delta = -delta;
      }
      if (delta > 3 || p == 0 || p == max) {
        this.setAttribute("value", p);
        // Fire DOM event so that accessible value change events occur
        let event = document.createEvent("Events");
        event.initEvent("ValueChange", true, true);
        this.dispatchEvent(event);
      }
    }

    return val;
  }

  get max() {
    return this.getAttribute("max") || "100";
  }

  set max(val) {
    this.setAttribute("max", isNaN(val) ? 100 : Math.max(val, 1));
    this.value = this.value;
    return val;
  }

  isUndetermined() {
    return this.getAttribute("mode") == "undetermined";
  }

  connectedCallback() {
    if (this.delayConnectedCallback()) {
      return;
    }
    this._initUI();
  }

  static get observedAttributes() {
    return [ "mode" ];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (!this.isConnectedAndReady) {
      return;
    }

    if (name === "mode" && oldValue != newValue) {
      this._initUI();
    }
  }

  _initUI() {
    let content = `
      <spacer class="progress-bar"/>
      <spacer class="progress-remainder"/>
    `;

    this.textContent = "";
    this.appendChild(MozXULElement.parseXULToFragment(content));
  }
}

customElements.define("progressmeter", MozProgressmeter);

}
