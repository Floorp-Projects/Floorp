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

    document.l10n.connectRoot(this.shadowRoot);
    document.l10n.translateFragment(this.shadowRoot);

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
    let labelId = this.item.labelId;

    // Map specific elements to a different message ID, to allow updates to
    // existing labels and avoid duplicates.
    let stringMapping = {
      signaturealgorithm: "signature-algorithm",
      "rfc-822-name": "email-address",
    };
    let fluentID = stringMapping[labelId] || labelId;

    label.setAttribute("data-l10n-id", "certificate-viewer-" + fluentID);

    this.classList.add(labelId);

    let info = this.shadowRoot.querySelector(".info");
    if (this.item.info.hasOwnProperty("utc")) {
      this.handleTimeZone(info);
      return;
    }
    if (labelId === "other-name") {
      info.setAttribute("data-l10n-id", "certificate-viewer-unsupported");
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

    this.classList.add(labelId);

    if (labelId === "modulus" || labelId === "public-value") {
      info.classList.add("long-hex");
      this.addEventListener("mouseup", () => {
        // If a range of text is selected, don't toggle the class that
        // hides/shows additional text.
        if (window.getSelection().type !== "Range") {
          info.classList.toggle("long-hex-open");
        }
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

    if (labelId === "download") {
      this.setDownloadLinkInformation(info);
    }
  }

  setDownloadLinkInformation(info) {
    let link = document.createElement("a");
    link.setAttribute("href", "data:," + this.item.info);
    link.classList.add("download-link");

    let url = new URL(document.URL);
    let certArray = url.searchParams.getAll("cert");
    let encodedCertArray = [];
    for (let i = 0; i < certArray.length; i++) {
      encodedCertArray.push(
        encodeURI(
          `-----BEGIN CERTIFICATE-----\r\n${certArray[i]}\r\n-----END CERTIFICATE-----\r\n`
        )
      );
    }
    encodedCertArray = encodedCertArray.join("");

    let chainLink = document.createElement("a");
    chainLink.setAttribute("href", "data:," + encodedCertArray);
    chainLink.classList.add("download-link");
    chainLink.classList.add("download-link-chain");

    info.textContent = "";
    info.appendChild(link);
    info.appendChild(chainLink);

    let commonName = document
      .querySelector("certificate-section")
      .shadowRoot.querySelector(".subject-name")
      .shadowRoot.querySelector(".common-name")
      .shadowRoot.querySelector(".info");

    let fileName = normalizeToKebabCase(commonName.textContent);

    document.l10n.setAttributes(link, "certificate-viewer-download-pem", {
      fileName,
    });

    document.l10n.setAttributes(
      chainLink,
      "certificate-viewer-download-pem-chain",
      {
        fileName,
      }
    );
  }
}

customElements.define("info-item", InfoItem);
