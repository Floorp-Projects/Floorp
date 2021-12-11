/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ErrorSection extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
    let template = document.getElementById("error-section-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" }).appendChild(templateHtml);

    document.l10n.connectRoot(this.shadowRoot);
    this.render();
  }

  render() {
    let title = this.shadowRoot.querySelector(".title");
    title.setAttribute("data-l10n-id", "certificate-viewer-error-title");

    let errorMessage = this.shadowRoot.querySelector(".error");
    errorMessage.setAttribute(
      "data-l10n-id",
      "certificate-viewer-error-message"
    );
  }
}
customElements.define("error-section", ErrorSection);
