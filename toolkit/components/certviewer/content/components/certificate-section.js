/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ErrorSection } from "./error-section.js";
import { InfoGroupContainer } from "./info-group-container.js";
import { CertificateTabsSection } from "./certificate-tabs-section.js";

class CertificateSection extends HTMLElement {
  constructor(certs, error) {
    super();
    this.certs = certs;
    this.error = error;
  }

  connectedCallback() {
    let template = document.getElementById("certificate-section-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" }).appendChild(templateHtml);

    document.l10n.connectRoot(this.shadowRoot);
    document.l10n.translateFragment(this.shadowRoot);

    this.certificateTabsSection = new CertificateTabsSection();
    this.shadowRoot.appendChild(this.certificateTabsSection.tabsElement);
    this.infoGroupsContainers = new InfoGroupContainer();

    this.render();
  }

  render() {
    let title = this.shadowRoot.querySelector(".title");
    title.setAttribute(
      "data-l10n-id",
      "certificate-viewer-certificate-section-title"
    );

    if (this.error) {
      title.classList.add("error");
      this.certificateTabsSection.appendChild(new ErrorSection());
      return;
    }
    let final = false;
    for (let i = 0; i < this.certs.length; i++) {
      if (i === this.certs.length - 1) {
        final = true;
      }
      this.infoGroupsContainers.createInfoGroupsContainers(
        this.certs[i].certItems,
        i,
        final
      );
      this.shadowRoot.appendChild(
        this.infoGroupsContainers.infoGroupsContainers[i]
      );
      this.certificateTabsSection.createTabSection(this.certs[i].tabName, i);
      this.infoGroupsContainers.addClass("selected", 0);
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
customElements.define("certificate-section", CertificateSection);
