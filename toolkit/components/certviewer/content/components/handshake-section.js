/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ReflectedFluentElement, handshakeArray, InfoItem */

class HandshakeSection extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
    let template = document.getElementById("handshake-section-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" }).appendChild(templateHtml);

    this.render();
  }

  render() {
    let title = this.shadowRoot.querySelector(".title");
    // TODO: Use fluent instead.
    title.innerContent = "Handshake";

    for (let i = 0; i < handshakeArray.length; i++) {
      this.shadowRoot.append(new InfoItem(handshakeArray[i]));
    }
  }
}
customElements.define("handshake-section", HandshakeSection);
