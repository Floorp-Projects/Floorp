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
    this._initUI();
  }

  disconnectedCallback() {
    this.runAnimation = false;
  }

  static get observedAttributes() {
    return [ "mode" ];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (name === "mode" && oldValue != newValue) {
      this._initUI();
    }
  }

  _initUI() {
    let isUndetermined = this.isUndetermined();
    let content = isUndetermined ?
      `
        <stack class="progress-remainder" flex="1" style="overflow: -moz-hidden-unscrollable;">
          <spacer class="progress-bar" top="0" style="margin-right: -1000px;"/>
        </stack>
      ` :
      `
        <spacer class="progress-bar"/>
        <spacer class="progress-remainder"/>
      `;

    this._stack = null;
    this._spacer = null;
    this._runAnimation = isUndetermined;

    this.textContent = "";
    this.appendChild(MozXULElement.parseXULToFragment(content));

    if (!isUndetermined) {
      return;
    }

    this._stack = this.querySelector(".progress-remainder");
    this._spacer = this.querySelector(".progress-bar");
    this._isLTR = document.defaultView.getComputedStyle(this).direction == "ltr";
    this._startTime = window.performance.now();

    let nextStep = (t) => {
      if (!this._runAnimation) {
        return;
      }

      let width = this._stack.boxObject.width;
      if (width) {
        let elapsedTime = t - this._startTime;

        // Width of chunk is 1/5 (determined by the ratio 2000:400) of the
        // total width of the progress bar. The left edge of the chunk
        // starts at -1 and moves all the way to 4. It covers the distance
        // in 2 seconds.
        let position = this._isLTR ? ((elapsedTime % 2000) / 400) - 1 :
                                     ((elapsedTime % 2000) / -400) + 4;

        width = width >> 2;
        this._spacer.height = this._stack.boxObject.height;
        this._spacer.width = width;
        this._spacer.left = width * position;
      }

      window.requestAnimationFrame(nextStep);
    };

    window.requestAnimationFrame(nextStep);
  }
}

customElements.define("progressmeter", MozProgressmeter);

}
