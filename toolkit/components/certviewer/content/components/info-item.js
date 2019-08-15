/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    let labelText = this.item.label
      .replace(/\s+/g, "-")
      .replace(/\./g, "")
      .replace(/\//g, "")
      .replace(/--/g, "-")
      .toLowerCase();
    label.setAttribute("data-l10n-id", "certificate-viewer-" + labelText);

    this.classList.add(labelText);

    this.classList.add(labelText);

    let info = this.shadowRoot.querySelector(".info");
    info.textContent = this.item.info;

    // TODO: Use Fluent-friendly condition.
    if (this.item.label === "Modulus") {
      info.classList.add("long-hex");
    }

    if (labelText === "download") {
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
          `-----BEGIN CERTIFICATE-----\r\n${
            certArray[i]
          }\r\n-----END CERTIFICATE-----\r\n`
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

    document.l10n.setAttributes(link, "certificate-viewer-download-pem", {
      domain: commonName.textContent,
    });

    document.l10n.setAttributes(
      chainLink,
      "certificate-viewer-download-pem-chain",
      {
        domain: commonName.textContent,
      }
    );
  }
}

customElements.define("info-item", InfoItem);
