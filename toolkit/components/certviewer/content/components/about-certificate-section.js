/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

import { InfoGroupContainer } from "./info-group-container.js";
import { CertificateTabsSection } from "./certificate-tabs-section.js";

const TYPE_UNKNOWN = 0;
const TYPE_CA = 1;
const TYPE_USER = 2;
const TYPE_EMAIL = 4;
const TYPE_SERVER = 8;

export class AboutCertificateSection extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
    let template = document.getElementById("about-certificate-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" }).appendChild(templateHtml);

    document.l10n.connectRoot(this.shadowRoot);

    this.certificateTabsSection = new CertificateTabsSection(true);
    this.shadowRoot.appendChild(this.certificateTabsSection.tabsElement);
    this.infoGroupsContainers = new InfoGroupContainer(true);

    this.render();
  }

  render() {
    RPMAddMessageListener("certificates", this.filterCerts.bind(this));
    RPMSendAsyncMessage("getCertificates");

    let title = this.shadowRoot.querySelector(".title");
    title.setAttribute(
      "data-l10n-id",
      "certificate-viewer-certificate-section-title"
    );
  }

  filterCerts(message) {
    let certs = [];
    if (message.data.certs[TYPE_USER].length) {
      certs.push({
        name: "certificate-viewer-tab-mine",
        data: message.data.certs[TYPE_USER],
      });
    }
    if (message.data.certs[TYPE_EMAIL].length) {
      certs.push({
        name: "certificate-viewer-tab-people",
        data: message.data.certs[TYPE_EMAIL],
      });
    }
    if (message.data.certs[TYPE_SERVER].length) {
      certs.push({
        name: "certificate-viewer-tab-servers",
        data: message.data.certs[TYPE_SERVER],
      });
    }
    if (message.data.certs[TYPE_CA].length) {
      certs.push({
        name: "certificate-viewer-tab-ca",
        data: message.data.certs[TYPE_CA],
      });
    }
    if (message.data.certs[TYPE_UNKNOWN].length) {
      certs.push({
        name: "certificate-viewer-tab-unkonwn",
        data: message.data.certs[TYPE_UNKNOWN],
      });
    }

    let i = 0;
    for (let cert of certs) {
      let final = i == certs.length - 1;
      this.infoGroupsContainers.createInfoGroupsContainers({}, i, final, cert);
      this.shadowRoot.appendChild(
        this.infoGroupsContainers.infoGroupsContainers[i]
      );
      this.certificateTabsSection.createTabSection(cert.name, i);
      this.infoGroupsContainers.addClass("selected", 0);
      i++;
    }
    this.setAccessibilityEventListeners();
    this.addClassForPadding();
  }

  // Adds class selector for items that need padding,
  // as nth-child/parent-based selectors aren't supported
  // due to the encapsulation of custom-element CSS.
  addClassForPadding() {
    let embeddedScts = this.shadowRoot.querySelector(".embedded-scts");
    if (!embeddedScts) {
      return;
    }
    let items = embeddedScts.shadowRoot.querySelectorAll(".version");

    for (let i = 0; i < items.length; i++) {
      items[i].classList.add("padding");
    }
  }

  setAccessibilityEventListeners() {
    this.certificateTabsSection.setAccessibilityEventListeners();
  }

  updateSelectedTab(index) {
    this.certificateTabsSection.updateSelectedTab(index);
  }

  updateCertificateSource(index) {
    this.infoGroupsContainers.updateCertificateSource(index);
  }
}
customElements.define("about-certificate-section", AboutCertificateSection);
