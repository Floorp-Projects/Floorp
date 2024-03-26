/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export default class SearchInput extends MozLitElement {
  static get properties() {
    return {
      items: { type: Array },
      change: { type: Function },
      value: { type: String },
    };
  }

  render() {
    return html` <link
        rel="stylesheet"
        href="chrome://global/content/megalist/megalist.css"
      />
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <input
        class="search"
        type="search"
        data-l10n-id="filter-placeholder"
        @input=${this.change}
        .value=${this.value}
      />`;
  }
}

customElements.define("search-input", SearchInput);
