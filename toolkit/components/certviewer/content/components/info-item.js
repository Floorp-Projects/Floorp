/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { normalizeToKebabCase } from "../utils.js";

export class InfoItem extends HTMLElement {
  constructor(item) {
    super();
    this.item = item;
  }

  connectedCallback() {
    let infoItemTemplate = document.getElementById("info-item-template");

    this.attachShadow({ mode: "open" }).appendChild(
      infoItemTemplate.content.cloneNode(true)
    );

    document.l10n.translateFragment(this.shadowRoot);
    document.l10n.connectRoot(this.shadowRoot);

    this.render();
  }

  render() {
    let label = this.shadowRoot.querySelector("label");
    let labelText = normalizeToKebabCase(this.item.label);
    label.setAttribute("data-l10n-id", "certificate-viewer-" + labelText);

    this.classList.add(labelText);

    let info = this.shadowRoot.querySelector(".info");
    info.textContent = this.item.info;

    // TODO: Use Fluent-friendly condition.
    if (this.item.label === "Modulus") {
      info.classList.add("long-hex");
      this.addEventListener("click", () => {
        info.classList.toggle("long-hex-open");
      });
    }
  }
}

customElements.define("info-item", InfoItem);
