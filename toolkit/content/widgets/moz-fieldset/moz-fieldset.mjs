/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

/**
 * Fieldset wrapper to lay out form inputs consistently.
 *
 * @tagname moz-fieldset
 * @property {string} label - The label for the fieldset's legend.
 * @property {string} description - The description for the fieldset.
 */
export default class MozFieldset extends MozLitElement {
  static properties = {
    label: { type: String },
    description: { type: String },
  };

  connectedCallback() {
    super.connectedCallback();
    this.dataset.l10nAttrs = "label, description";
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-fieldset.css"
      />
      <fieldset
        aria-describedby=${ifDefined(this.description ? "description" : null)}
      >
        <legend class="heading-medium">${this.label}</legend>
        ${this.description
          ? html`<p id="description" class="text-deemphasized">
              ${this.description}
            </p>`
          : ""}
        <div id="inputs">
          <slot></slot>
        </div>
      </fieldset>
    `;
  }
}
customElements.define("moz-fieldset", MozFieldset);
