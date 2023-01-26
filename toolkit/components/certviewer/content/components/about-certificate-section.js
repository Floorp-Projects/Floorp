/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { InfoGroupContainer } from "./info-group-container.js";
import { CertificateTabsSection } from "./certificate-tabs-section.js";

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
    RPMSendQuery("getCertificates").then(this.filterCerts.bind(this));

    let title = this.shadowRoot.querySelector(".title");
    title.setAttribute(
      "data-l10n-id",
      "certificate-viewer-certificate-section-title"
    );
  }

  filterCerts(srcCerts) {
    let certs = [];
    if (srcCerts[TYPE_USER].length) {
      certs.push({
        name: "certificate-viewer-tab-mine",
        data: srcCerts[TYPE_USER],
      });
    }
    if (srcCerts[TYPE_EMAIL].length) {
      certs.push({
        name: "certificate-viewer-tab-people",
        data: srcCerts[TYPE_EMAIL],
      });
    }
    if (srcCerts[TYPE_SERVER].length) {
      certs.push({
        name: "certificate-viewer-tab-servers",
        data: srcCerts[TYPE_SERVER],
      });
    }
    if (srcCerts[TYPE_CA].length) {
      certs.push({
        name: "certificate-viewer-tab-ca",
        data: srcCerts[TYPE_CA],
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
    let items = embeddedScts.shadowRoot.querySelectorAll(".timestamp");

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
