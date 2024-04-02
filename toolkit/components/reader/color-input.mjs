/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * @tagname color-input
 * @property {string} color - The initial color value as a hex code.
 * @property {string} propName - The property that the color input sets.
 * @property {string} l10nId - l10nId for label text.
 */
export default class ColorInput extends MozLitElement {
  static properties = {
    color: { type: String },
    propName: { type: String, attribute: "prop-name" },
    l10nId: { type: String, attribute: "data-l10n-id" },
  };

  static queries = {
    inputEl: "#color-swatch",
  };

  handleColorInput(event) {
    this.color = event.target.value;
    this.dispatchEvent(
      new CustomEvent("color-picked", {
        detail: this.color,
      })
    );
  }

  /* Function to launch color picker when the user clicks anywhere in the container. */
  handleClick(event) {
    // If the user directly clicks the color swatch, no need to propagate click.
    if (event.target.matches("input")) {
      return;
    }
    this.inputEl.click();
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/reader/color-input.css"
      />
      <div class="color-input-container" @click="${this.handleClick}">
        <input
          type="color"
          name="${this.propName}"
          .value="${this.color}"
          id="color-swatch"
          @input="${this.handleColorInput}"
        />
        <label for="color-swatch" data-l10n-id=${this.l10nId}></label>
        <div class="icon-container">
          <img
            class="icon"
            role="presentation"
            src="chrome://global/skin/icons/edit-outline.svg"
          />
        </div>
      </div>
    `;
  }
}
customElements.define("color-input", ColorInput);
