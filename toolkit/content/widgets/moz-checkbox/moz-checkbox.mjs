/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-label.mjs";

/**
 * A checkbox input with a label.
 *
 * @tagname moz-checkbox
 * @property {string} label - The text of the label element
 * @property {boolean} checked - The state of the checkbox element,
 *  also controls whether the checkbox is initially rendered as
 *  being checked
 */
export default class MozCheckbox extends MozLitElement {
  static properties = {
    label: { type: String },
    iconSrc: { type: String },
    checked: { type: Boolean, reflect: true },
    disabled: { type: Boolean, reflect: true },
  };

  static get queries() {
    return {
      checkboxEl: "#moz-checkbox",
      labelEl: "label",
      icon: ".icon",
    };
  }

  constructor() {
    super();
    this.checked = false;
    this.disabled = false;
  }

  connectedCallback() {
    super.connectedCallback();
    this.dataset.l10nAttrs = "label";
  }

  focus() {
    this.checkboxEl.focus();
  }

  /**
   * Handles click events and keeps the checkbox checked value in sync
   *
   * @param {Event} event
   * @memberof MozCheckbox
   */
  handleStateChange(event) {
    this.checked = event.target.checked;
  }

  /**
   * Dispatches an event from the host element so that outside
   * listeners can react to these events
   *
   * @param {Event} event
   * @memberof MozCheckbox
   */
  redispatchEvent(event) {
    let newEvent = new Event(event.type, event);
    this.dispatchEvent(newEvent);
  }

  iconTemplate() {
    if (this.iconSrc) {
      return html`<img src=${this.iconSrc} role="presentation" class="icon" />`;
    }
    return "";
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-checkbox.css"
      />
      <label is="moz-label" for="moz-checkbox">
        <input
          id="moz-checkbox"
          type="checkbox"
          .checked=${this.checked}
          @click=${this.handleStateChange}
          @change=${this.redispatchEvent}
          .disabled=${this.disabled}
        />
        <span class="label-content">
          ${this.iconTemplate()}
          <span class="text">${this.label}</span>
        </span>
      </label>
    `;
  }
}
customElements.define("moz-checkbox", MozCheckbox);
