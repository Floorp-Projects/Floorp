/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { b64ToPEM, normalizeToKebabCase } from "./utils.js";

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
    infoElement.textContent = UTCTime;
    infoElement.setAttribute("title", localTime);
  }

  addLongHexOverflow(info) {
    info.classList.add("hex");

    // For visual appeal, we want to collapse large hex values into single
    // line items that can be clicked to expand.
    // This function measures the size of the info item relative to its
    // container and adds the "long-hex" class if it's overflowing. Since the
    // container size changes on window resize this function is hooked up to
    // a resize event listener.
    function resize() {
      if (info.classList.contains("hex-open")) {
        info.classList.toggle("long-hex", true);
        return;
      }

      // If the item is not currently drawn and we can't measure its dimensions
      // then attach an observer that will measure it once it appears.
      if (info.clientWidth <= 0) {
        let observer = new IntersectionObserver(function([
          { intersectionRatio },
        ]) {
          if (intersectionRatio > 0) {
            info.classList.toggle(
              "long-hex",
              info.scrollWidth > info.clientWidth
            );
            observer.unobserve(info);
          }
        },
        {});

        observer.observe(info);
      }
      info.classList.toggle("long-hex", info.scrollWidth > info.clientWidth);
    }
    window.addEventListener("resize", resize);
    window.requestAnimationFrame(resize);

    this.addEventListener("mouseup", () => {
      // If a range of text is selected, don't toggle the class that
      // hides/shows additional text.
      if (
        info.classList.contains("long-hex") &&
        window.getSelection().type !== "Range"
      ) {
        info.classList.toggle("hex-open");
      }
    });
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

    if (this.item.isHex) {
      this.addLongHexOverflow(info);
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
      info.classList.add("url");
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
      encodedCertArray.push(encodeURI(b64ToPEM(certArray[i])));
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
