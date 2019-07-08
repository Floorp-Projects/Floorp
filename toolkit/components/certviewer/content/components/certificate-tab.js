/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals updateSelectedItem */

class CertificateTab extends HTMLElement {
  constructor(id) {
    super();
    this.id = id;
  }

  connectedCallback() {
    let template = document.getElementById("certificate-tab-template");

    this.attachShadow({ mode: "open" }).appendChild(
      template.content.cloneNode(true)
    );

    this.render();
    this.addEventListener("click", e => this.setSelectedTab(e));
  }

  render() {
    let tab = this.shadowRoot.querySelector(".tab");
    tab.textContent = "tab" + this.id;
  }

  setSelectedTab(e) {
    updateSelectedItem(this.id);
  }
}

customElements.define("certificate-tab", CertificateTab);
