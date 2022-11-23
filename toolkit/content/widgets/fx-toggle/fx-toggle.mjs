/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at htp://mozilla.org/MPL/2.0/. */

// Bug 1790483: Lit is not bundled yet, this is dev only.
// eslint-disable-next-line import/no-unresolved
import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

export default class FxToggle extends MozLitElement {
  static LOCAL_NAME = "fx-toggle";

  static properties = {
    checked: { type: Boolean, reflect: true },
    disabled: { type: Boolean, reflect: true },
    label: { type: String },
    description: { type: String },
    ariaLabel: { type: String, attribute: "aria-label" },
  };

  static get queries() {
    return {
      inputEl: "#fx-toggle-input",
      labelEl: "#fx-toggle-label",
      descriptionEl: "#fx-toggle-description",
    };
  }

  constructor() {
    super();
    this.checked = false;
    this.disabled = false;
  }

  handleChange() {
    this.checked = this.inputEl.checked;
    this.dispatchEvent(
      new Event("change", {
        bubbles: true,
        composed: true,
      })
    );
  }

  labelTemplate() {
    if (this.label) {
      return html`
        <label id="fx-toggle-label" part="label" for="fx-toggle-input">
          ${this.label}
        </label>
      `;
    }
    return "";
  }

  descriptionTemplate() {
    if (this.description) {
      return html`
        <p id="fx-toggle-description" part="description">
          ${this.description}
        </p>
      `;
    }
    return "";
  }

  render() {
    const { checked, disabled, description, ariaLabel, handleChange } = this;
    return html`
      <link rel="stylesheet" href=${this.constructor.stylesheetUrl} />
      ${this.labelTemplate()} ${this.descriptionTemplate()}
      <input
        id="fx-toggle-input"
        part="input"
        type="checkbox"
        role="switch"
        class="toggle-button"
        .checked=${checked}
        ?disabled=${disabled}
        aria-checked=${checked}
        aria-label=${ifDefined(ariaLabel ?? undefined)}
        aria-describedby=${ifDefined(
          description ? "fx-toggle-description" : undefined
        )}
        @change=${handleChange}
      />
    `;
  }
}
customElements.define("fx-toggle", FxToggle);
