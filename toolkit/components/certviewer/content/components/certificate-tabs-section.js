/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { normalizeToKebabCase } from "../utils.js";
import { updateSelectedItem } from "../certviewer.js";

export class CertificateTabsSection extends HTMLElement {
  constructor(isAboutCertificate) {
    super();
    this.isAboutCertificate = isAboutCertificate || false;
    this.connectedCallback();
  }

  connectedCallback() {
    let certificateTabsTemplate = document.getElementById(
      "certificate-tabs-template"
    );
    this.attachShadow({ mode: "open" }).appendChild(
      certificateTabsTemplate.content.cloneNode(true)
    );
    this.render();
  }

  render() {
    this.tabsElement = this.shadowRoot.querySelector(".certificate-tabs");
  }

  appendChild(child) {
    this.tabsElement.appendChild(child);
  }

  createTabSection(tabName, i) {
    let tab = document.createElement("button");
    tab.textContent = tabName;
    tab.setAttribute("id", normalizeToKebabCase(tabName));
    tab.setAttribute("aria-controls", "panel" + i);
    tab.setAttribute("idnumber", i);
    tab.setAttribute("role", "tab");
    tab.classList.add("certificate-tab");
    tab.classList.add("tab");
    if (this.isAboutCertificate) {
      tab.setAttribute("data-l10n-id", tabName);
    }
    this.tabsElement.appendChild(tab);

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

  updateSelectedTab(index) {
    let tabs = this.tabsElement.querySelectorAll(".certificate-tab");

    for (let tab of tabs) {
      tab.classList.remove("selected");
    }
    tabs[index].classList.add("selected");
  }

  /* Information on setAccessibilityEventListeners() can be found
   * at https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/Tab_Role */
  setAccessibilityEventListeners() {
    let tabs = this.tabsElement.querySelectorAll('[role="tab"]');

    // Add a click event handler to each tab
    for (let tab of tabs) {
      tab.addEventListener("click", e =>
        updateSelectedItem(e.target.getAttribute("idnumber"))
      );
    }

    // Enable arrow navigation between tabs in the tab list
    let tabFocus = 0;

    this.tabsElement.addEventListener("keydown", e => {
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
}

customElements.define("certificate-tabs-section", CertificateTabsSection);
