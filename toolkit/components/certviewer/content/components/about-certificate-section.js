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
  constructor(certs) {
    super();
    this.certs = certs;
  }

  connectedCallback() {
    let template = document.getElementById("about-certificate-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" }).appendChild(templateHtml);

    document.l10n.connectRoot(this.shadowRoot);

    this.certificateTabsSection = new CertificateTabsSection(true);
    this.shadowRoot.appendChild(this.certificateTabsSection.tabsElement);
    this.infoGroupsContainers = new InfoGroupContainer(true);

    this.certData = {
      [TYPE_UNKNOWN]: {
        name: "certificate-viewer-tab-unkonwn",
        data: null,
      },
      [TYPE_CA]: {
        name: "certificate-viewer-tab-ca",
        data: null,
      },
      [TYPE_USER]: {
        name: "certificate-viewer-tab-mine",
        data: null,
      },
      [TYPE_EMAIL]: {
        name: "certificate-viewer-tab-people",
        data: null,
      },
      [TYPE_SERVER]: {
        name: "certificate-viewer-tab-servers",
        data: null,
      },
    };

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
    this.certData[TYPE_UNKNOWN].data = message.data.certs[TYPE_UNKNOWN];
    this.certData[TYPE_CA].data = message.data.certs[TYPE_CA];
    this.certData[TYPE_USER].data = message.data.certs[TYPE_USER];
    this.certData[TYPE_EMAIL].data = message.data.certs[TYPE_EMAIL];
    this.certData[TYPE_SERVER].data = message.data.certs[TYPE_SERVER];

    let final = false;
    let i = 0;
    for (let data of Object.values(this.certData)) {
      if (i === this.certs.length - 1) {
        final = true;
      }
      this.infoGroupsContainers.createInfoGroupsContainers({}, i, final, data);
      this.shadowRoot.appendChild(
        this.infoGroupsContainers.infoGroupsContainers[i]
      );
      this.certificateTabsSection.createTabSection(data.name, i);
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
