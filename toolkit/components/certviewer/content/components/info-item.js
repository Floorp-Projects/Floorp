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

  handleTimeZone(infoElement) {
    let localTime = this.item.info.local;
    let UTCTime = this.item.info.utc;
    infoElement.textContent = localTime;
    infoElement.setAttribute("title", UTCTime);
  }

  render() {
    let label = this.shadowRoot.querySelector("label");
    let labelText = normalizeToKebabCase(this.item.label);

    // Map specific elements to a different message ID, to allow updates to
    // existing labels and avoid duplicates.
    let stringMapping = {
      signaturealgorithm: "signature-algorithm",
    };
    let fluentID = stringMapping[labelText] || labelText;

    label.setAttribute("data-l10n-id", "certificate-viewer-" + fluentID);

    this.classList.add(labelText);

    let info = this.shadowRoot.querySelector(".info");
    if (this.item.info.hasOwnProperty("utc")) {
      this.handleTimeZone(info);
      return;
    }
    if (typeof this.item.info === "boolean") {
      document.l10n.setAttributes(info, "certificate-viewer-boolean", {
        boolean: this.item.info,
      });
    } else {
      info.textContent = Array.isArray(this.item.info)
        ? this.item.info.join(", ")
        : this.item.info;
    }

    this.classList.add(labelText);

    // TODO: Use Fluent-friendly condition.
    if (this.item.label === "Modulus") {
      info.classList.add("long-hex");
      this.addEventListener("click", () => {
        info.classList.toggle("long-hex-open");
      });
    }

    let isURL = false;
    if (
      typeof this.item.info === "string" &&
      this.item.info.startsWith("http")
    ) {
      try {
        new URL(this.item.info);
        isURL = true;
      } catch (e) {}
    }

    if (isURL) {
      let link = document.createElement("a");
      link.setAttribute("href", this.item.info);
      link.setAttribute("rel", "noreferrer noopener");
      link.setAttribute("target", "_blank");
      link.textContent = this.item.info;
      info.textContent = "";
      info.appendChild(link);
    }
  }
}

customElements.define("info-item", InfoItem);
