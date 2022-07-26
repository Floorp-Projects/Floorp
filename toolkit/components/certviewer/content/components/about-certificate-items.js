/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { ListItem } from "./list-item.js";

export class AboutCertificateItems extends HTMLElement {
  constructor(id, data) {
    super();
    this.id = id;
    this.data = data;
  }

  connectedCallback() {
    let template = document.getElementById("about-certificate-items-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" }).appendChild(templateHtml);

    document.l10n.connectRoot(this.shadowRoot);

    this.render();
  }

  render() {
    for (let cert of this.data) {
      this.shadowRoot.append(new ListItem(cert));
    }
  }
}
customElements.define("about-certificate-items", AboutCertificateItems);
