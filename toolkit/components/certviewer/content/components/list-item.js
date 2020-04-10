/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { normalizeToKebabCase } from "../utils.js";

export class ListItem extends HTMLElement {
  constructor(item) {
    super();
    this.item = item;
  }

  connectedCallback() {
    let ListItemTemplate = document.getElementById("list-item-template");

    this.attachShadow({ mode: "open" }).appendChild(
      ListItemTemplate.content.cloneNode(true)
    );

    document.l10n.translateFragment(this.shadowRoot);
    document.l10n.connectRoot(this.shadowRoot);

    this.render();
  }

  render() {
    let label = this.shadowRoot.querySelector(".item-name");
    label.textContent = this.item.displayName;

    this.handleExport();

    let link = this.shadowRoot.querySelector(".cert-url");
    let derb64 = encodeURIComponent(this.item.derb64);
    let url = `about:certificate?cert=${derb64}`;
    link.setAttribute("href", url);
  }

  handleExport() {
    let exportButton = this.shadowRoot.querySelector(".export");
    // Wrap the Base64 string into lines of 64 characters,
    // with CRLF line breaks (as specified in RFC 1421).
    let wrapped = this.item.derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
    let download =
      "-----BEGIN CERTIFICATE-----\r\n" +
      wrapped +
      "\r\n-----END CERTIFICATE-----\r\n";

    let element = document.createElement("a");
    element.setAttribute("href", "data:," + encodeURI(download));
    let fileName = normalizeToKebabCase(this.item.displayName);
    document.l10n.setAttributes(element, "certificate-viewer-export", {
      fileName,
    });
    exportButton.appendChild(element);
  }
}

customElements.define("list-item", ListItem);
