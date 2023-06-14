/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { InfoItem } from "./info-item.mjs";
import { updateSelectedItem } from "../certviewer.mjs";
import { normalizeToKebabCase } from "./utils.mjs";

export class InfoGroup extends HTMLElement {
  constructor(item, final) {
    super();
    this.item = item;
    this.final = final;
  }

  connectedCallback() {
    // Attach and connect before adding the template, or fluent
    // won't translate the template copy we insert into the
    // shadowroot.
    this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(this.shadowRoot);

    let infoGroupTemplate = document.getElementById("info-group-template");
    this.shadowRoot.appendChild(infoGroupTemplate.content.cloneNode(true));
    this.render();
  }

  render() {
    let title = this.shadowRoot.querySelector(".info-group-title");
    document.l10n.setAttributes(
      title,
      `certificate-viewer-${this.item.sectionId}`
    );

    // Adds a class with the section title's name, to make
    // it easier to find when highlighting errors.
    this.classList.add(this.item.sectionId);
    for (let i = 0; i < this.item.sectionItems.length; i++) {
      this.shadowRoot.append(new InfoItem(this.item.sectionItems[i]));
    }

    if (this.item.sectionId === "issuer-name") {
      this.setLinkToTab();
    }

    let criticalIcon = this.shadowRoot.querySelector("#critical-info");
    if (!this.item.Critical) {
      criticalIcon.style.display = "none";
    }
  }

  setLinkToTab() {
    if (this.final) {
      return;
    }

    let issuerLabelElement =
      this.shadowRoot.querySelector(".common-name") ||
      this.shadowRoot.querySelector(".organizational-unit");

    issuerLabelElement = issuerLabelElement?.shadowRoot.querySelector(".info");

    if (!issuerLabelElement) {
      return;
    }

    let link = document.createElement("a");
    link.textContent = issuerLabelElement.textContent;
    if (!link.textContent) {
      link.setAttribute(
        "data-l10n-id",
        "certificate-viewer-unknown-group-label"
      );
    }
    link.setAttribute("href", "#");

    issuerLabelElement.textContent = "";
    issuerLabelElement.appendChild(link);

    link.addEventListener("click", () => {
      let id = normalizeToKebabCase(link.textContent);
      let issuerTab = document
        .querySelector("certificate-section")
        .shadowRoot.getElementById(id);

      let index = issuerTab.getAttribute("idnumber");

      updateSelectedItem(index);
    });
  }
}

customElements.define("info-group", InfoGroup);
