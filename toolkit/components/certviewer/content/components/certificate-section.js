/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals certArray, updateSelectedItem, InfoGroup */

class CertificateSection extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
    let template = document.getElementById("certificate-section-template");
    let templateHtml = template.content.cloneNode(true);

    this.attachShadow({ mode: "open" }).appendChild(templateHtml);

    this.infoGroupsContainers = [];
    this.createInfoGroupsContainers();
    this.render();
  }

  render() {
    let certificateTabs = this.shadowRoot.querySelector(".certificate-tabs");
    let title = this.shadowRoot.querySelector(".title");
    title.textContent = "Certificate";
    this.infoGroupContainer = this.shadowRoot.querySelector(".info-groups");
    for (let i = 0; i < certArray.length; i++) {
      let tab = document.createElement("button");
      tab.textContent = "tab" + i;
      tab.setAttribute("id", "tab" + i);
      tab.setAttribute("aria-controls", "panel" + i);
      tab.setAttribute("idnumber", i);
      tab.setAttribute("role", "tab");
      tab.classList.add("certificate-tab");
      tab.classList.add("tab");
      certificateTabs.appendChild(tab);

      // If it is the first tab, allow it to be tabbable by the user.
      // If it isn't the first tab, do not allow tab functionality,
      // as arrow functionality is implemented in certviewer.js.
      if (i === 0) {
        tab.classList.add("selected");
        tab.setAttribute("tabindex", 0);
      } else {
        tab.setAttribute("tabindex", -1);
      }
    }
    this.setAccessibilityEventListeners();
    this.infoGroupsContainers[0].classList.add("selected");
  }

  /* Information on setAccessibilityEventListeners() can be found
   * at https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/Tab_Role */
  setAccessibilityEventListeners() {
    const tabs = this.shadowRoot.querySelectorAll('[role="tab"]');
    const tabList = this.shadowRoot.querySelector('[role="tablist"]');

    // Add a click event handler to each tab
    tabs.forEach(tab => {
      tab.addEventListener("click", e =>
        updateSelectedItem(e.target.getAttribute("idnumber"))
      );
    });

    // Enable arrow navigation between tabs in the tab list
    let tabFocus = 0;

    tabList.addEventListener("keydown", e => {
      // Move right
      if (e.keyCode === 39 || e.keyCode === 37) {
        // After navigating away from the current tab,
        // prevent that tab from being tabbable -
        // so as to only allow arrow navigation within the tablist.
        tabs[tabFocus].setAttribute("tabindex", -1);
        if (e.keyCode === 39) {
          tabFocus++;
          // If we're at the end, go to the start
          if (tabFocus > tabs.length - 1) {
            tabFocus = 0;
          }
          // Move left
        } else if (e.keyCode === 37) {
          tabFocus--;
          // If we're at the start, move to the end
          if (tabFocus < 0) {
            tabFocus = tabs.length;
          }
        }
        tabs[tabFocus].setAttribute("tabindex", 0);
        tabs[tabFocus].focus();
      }
    });
  }

  createInfoGroupsContainers() {
    for (let i = 0; i < certArray.length; i++) {
      this.infoGroupsContainers[i] = document.createElement("div");
      this.infoGroupsContainers[i].setAttribute("id", "panel" + i);
      this.infoGroupsContainers[i].setAttribute("role", "tabpanel");
      this.infoGroupsContainers[i].setAttribute("tabindex", 0);
      this.infoGroupsContainers[i].setAttribute("aria-labelledby", "tab" + i);
      if (i !== 0) {
        this.infoGroupsContainers[i].setAttribute("hidden", true);
      }
      this.infoGroupsContainers[i].classList.add("info-groups");
      this.shadowRoot.appendChild(this.infoGroupsContainers[i]);
      let arrayItem = certArray[i];
      for (let j = 0; j < arrayItem.length; j++) {
        this.infoGroupsContainers[i].appendChild(new InfoGroup(arrayItem[j]));
      }
    }
  }

  updateSelectedTab(index) {
    let tabs = this.shadowRoot.querySelectorAll(".certificate-tab");

    for (let i = 0; i < tabs.length; i++) {
      tabs[i].classList.remove("selected");
    }
    tabs[index].classList.add("selected");
  }

  updateCertificateSource(index) {
    for (let i = 0; i < this.infoGroupsContainers.length; i++) {
      this.infoGroupsContainers[i].classList.remove("selected");
    }
    this.infoGroupsContainers[index].classList.add("selected");
  }
}
customElements.define("certificate-section", CertificateSection);
