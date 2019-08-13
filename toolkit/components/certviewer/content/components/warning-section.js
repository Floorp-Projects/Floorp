/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class WarningSection extends HTMLElement {
  constructor(errorCode) {
    super();

    this.errorCode = errorCode;
  }

  connectedCallback() {
    let template = document.getElementById("warning-section-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" });

    document.l10n.connectRoot(this.shadowRoot);

    this.shadowRoot.appendChild(templateHtml);

    this.render();
  }

  render() {
    let warningMessage = this.errorCode.replace(/\s+/g, "-").toLowerCase();

    let messageElement = this.shadowRoot.querySelector(".warning-message");
    warningMessage = warningMessage.replace(/_/g, "-");
    messageElement.setAttribute(
      "data-l10n-id",
      "certificate-viewer-" + warningMessage
    );
  }
}
customElements.define("warning-section", WarningSection);
