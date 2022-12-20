/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at htp://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

export default class MozToggle extends MozLitElement {
  static properties = {
    pressed: { type: Boolean, reflect: true },
    disabled: { type: Boolean, reflect: true },
    label: { type: String },
    description: { type: String },
    ariaLabel: { type: String, attribute: "aria-label" },
  };

  static get queries() {
    return {
      buttonEl: "#moz-toggle-button",
      labelEl: "#moz-toggle-label",
      descriptionEl: "#moz-toggle-description",
    };
  }

  // Use a relative URL in storybook to get faster reloads on style changes.
  static stylesheetUrl = window.IS_STORYBOOK
    ? "./moz-toggle/moz-toggle.css"
    : "chrome://global/content/elements/moz-toggle.css";

  constructor() {
    super();
    this.pressed = false;
    this.disabled = false;
  }

  handleClick() {
    this.pressed = !this.pressed;
    this.dispatchOnUpdateComplete(
      new CustomEvent("toggle", {
        bubbles: true,
        composed: true,
      })
    );
  }

  labelTemplate() {
    if (this.label) {
      return html`
        <label id="moz-toggle-label" part="label" for="moz-toggle-button">
          ${this.label}
        </label>
      `;
    }
    return "";
  }

  descriptionTemplate() {
    if (this.description) {
      return html`
        <p id="moz-toggle-description" part="description">
          ${this.description}
        </p>
      `;
    }
    return "";
  }

  render() {
    const { pressed, disabled, description, ariaLabel, handleClick } = this;
    return html`
      <link rel="stylesheet" href=${this.constructor.stylesheetUrl} />
      ${this.labelTemplate()} ${this.descriptionTemplate()}
      <button
        id="moz-toggle-button"
        part="button"
        type="button"
        class="toggle-button"
        ?disabled=${disabled}
        aria-pressed=${pressed}
        aria-label=${ifDefined(ariaLabel ?? undefined)}
        aria-describedby=${ifDefined(
          description ? "moz-toggle-description" : undefined
        )}
        @click=${handleClick}
      ></button>
    `;
  }
}
customElements.define("moz-toggle", MozToggle);
