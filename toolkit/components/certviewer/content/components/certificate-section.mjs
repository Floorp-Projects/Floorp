/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ErrorSection } from "./error-section.mjs";
import { InfoGroupContainer } from "./info-group-container.mjs";
import { CertificateTabsSection } from "./certificate-tabs-section.mjs";

class CertificateSection extends HTMLElement {
  constructor(certs, error) {
    super();
    this.certs = certs;
    this.error = error;
  }

  connectedCallback() {
    // Attach and connect before adding the template, or fluent
    // won't translate the template copy we insert into the
    // shadowroot.
    this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(this.shadowRoot);

    let template = document.getElementById("certificate-section-template");
    let templateHtml = template.content.cloneNode(true);
    this.shadowRoot.appendChild(templateHtml);

    this.certificateTabsSection = new CertificateTabsSection();
    this.shadowRoot.appendChild(this.certificateTabsSection.tabsElement);
    this.infoGroupsContainers = new InfoGroupContainer();

    this.render();
  }

  render() {
    let title = this.shadowRoot.querySelector(".title");
    document.l10n.setAttributes(
      title,
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
customElements.define("certificate-section", CertificateSection);
