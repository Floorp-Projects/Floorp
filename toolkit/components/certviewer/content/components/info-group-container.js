/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { InfoGroup } from "./info-group.js";
import { AboutCertificateItems } from "./about-certificate-items.js";

export class InfoGroupContainer extends HTMLElement {
  constructor(isAboutCertificate = false) {
    super();
    this.infoGroupsContainers = [];
    this.isAboutCertificate = isAboutCertificate;
  }

  connectedCallback() {
    let infoGroupContainerTemplate = document.getElementById(
      "info-groups-template"
    );
    this.attachShadow({ mode: "open" }).appendChild(
      infoGroupContainerTemplate.content.cloneNode(true)
    );
    this.render();
  }

  render() {}

  createInfoGroupsContainers(certArray, i, final, certData = []) {
    this.infoGroupsContainers[i] = document.createElement("div");
    this.infoGroupsContainers[i].setAttribute("id", "panel" + i);
    this.infoGroupsContainers[i].setAttribute("role", "tabpanel");
    this.infoGroupsContainers[i].setAttribute("tabindex", 0);
    this.infoGroupsContainers[i].setAttribute("aria-labelledby", "tab" + i);
    // Hiding all the certificzte contents except for the first tab that is
    // selected and shown by default
    if (i !== 0) {
      this.infoGroupsContainers[i].setAttribute("hidden", true);
    }
    this.infoGroupsContainers[i].classList.add("info-groups");

    if (this.isAboutCertificate) {
      this.infoGroupsContainers[i].appendChild(
        new AboutCertificateItems(certData.name, certData.data)
      );
    } else {
      for (let j = 0; j < certArray.length; j++) {
        this.infoGroupsContainers[i].appendChild(
          new InfoGroup(certArray[j], final)
        );
      }
    }
  }

  addClass(className, index) {
    this.infoGroupsContainers[index].classList.add(className);
  }

  updateCertificateSource(index) {
    for (let i = 0; i < this.infoGroupsContainers.length; i++) {
      this.infoGroupsContainers[i].classList.remove("selected");
    }
    this.infoGroupsContainers[index].classList.add("selected");
  }
}

customElements.define("info-group-container", InfoGroupContainer);
