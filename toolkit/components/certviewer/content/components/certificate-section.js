/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals certArray, CertificateTab, InfoGroup */

class CertificateSection extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
    let template = document.getElementById("certificate-section-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" }).appendChild(templateHtml);

    this.infoGroupsContainers = [];
    this.createInfoGroupsContainers();
    this.render();
  }

  render() {
    let certificateTabs = this.shadowRoot.querySelector(".certificate-tabs");
    let title = this.shadowRoot.querySelector(".title");
    title.textContent = "Certificate";
    this.infoGroupContainer = this.shadowRoot.querySelector(".info-groups");
    for (let i = 0; i < certArray.length; i++) {
      let tab = certificateTabs.appendChild(new CertificateTab(i));
      if (i === 0) {
        tab.classList.add("selected");
      }
    }

    this.infoGroupsContainers[0].classList.add("selected");
  }

  createInfoGroupsContainers() {
    for (let i = 0; i < certArray.length; i++) {
      this.infoGroupsContainers[i] = document.createElement("div");
      this.infoGroupsContainers[i].classList.add("info-groups");
      this.shadowRoot.appendChild(this.infoGroupsContainers[i]);
      let arrayItem = certArray[i];
      for (let j = 0; j < arrayItem.length; j++) {
        this.infoGroupsContainers[i].appendChild(new InfoGroup(arrayItem[j]));
      }
    }
  }

  updateSelectedTab(index) {
    let tabs = this.shadowRoot.querySelectorAll("certificate-tab");

    for (let i = 0; i < tabs.length; i++) {
      tabs[i].classList.remove("selected");
    }
    tabs[index].classList.add("selected");
  }

  updateCertificateSource(index) {
    for (let i = 0; i < this.infoGroupsContainers.length; i++) {
      this.infoGroupsContainers[i].classList.remove("selected");
    }
    this.infoGroupsContainers[index].classList.add("selected");
  }
}
customElements.define("certificate-section", CertificateSection);
