/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { InfoItem } from "./info-item.js";
import { updateSelectedItem } from "../certviewer.js";
import { normalizeToKebabCase } from "../utils.js";

export class InfoGroup extends HTMLElement {
  constructor(item, final) {
    super();
    this.item = item;
    this.final = final;
  }

  connectedCallback() {
    let infoGroupTemplate = document.getElementById("info-group-template");
    this.attachShadow({ mode: "open" }).appendChild(
      infoGroupTemplate.content.cloneNode(true)
    );
    document.l10n.connectRoot(this.shadowRoot);
    document.l10n.translateFragment(this.shadowRoot);
    this.render();
  }

  render() {
    let title = this.shadowRoot.querySelector(".info-group-title");
    title.setAttribute(
      "data-l10n-id",
      "certificate-viewer-" + this.item.sectionId
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
  }

  setLinkToTab() {
    if (this.final) {
      return;
    }

    let issuerCommonName = this.shadowRoot
      .querySelector(".common-name")
      .shadowRoot.querySelector(".info");

    let link = document.createElement("a");
    link.textContent = issuerCommonName.textContent;
    link.setAttribute("href", "#");

    issuerCommonName.textContent = "";
    issuerCommonName.appendChild(link);

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
