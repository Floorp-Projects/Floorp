/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { InfoItem } from "./info-item.js";

export class InfoGroup extends HTMLElement {
  constructor(item) {
    super();
    this.item = item;
  }

  connectedCallback() {
    let infoGroupTemplate = document.getElementById("info-group-template");
    this.attachShadow({ mode: "open" }).appendChild(
      infoGroupTemplate.content.cloneNode(true)
    );
    this.render();
  }

  render() {
    let title = this.shadowRoot.querySelector(".info-group-title");
    title.textContent = this.item.sectionTitle;

    // Adds a class with the section title's name, to make
    // it easier to find when highlighting errors.
    this.classList.add(
      this.item.sectionTitle.replace(/\s+/g, "-").toLowerCase()
    );

    this.classList.add(
      this.item.sectionTitle.replace(/\s+/g, "-").toLowerCase()
    );

    for (let i = 0; i < this.item.sectionItems.length; i++) {
      this.shadowRoot.append(new InfoItem(this.item.sectionItems[i]));
    }
  }
}

customElements.define("info-group", InfoGroup);
