/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{

class MozTabbox extends MozXULElement {
  constructor() {
    super();
    this._handleMetaAltArrows = /Mac/.test(navigator.platform);
    this.disconnectedCallback = this.disconnectedCallback.bind(this);
  }

  connectedCallback() {
    Services.els.addSystemEventListener(document, "keydown", this, false);
    window.addEventListener("unload", this.disconnectedCallback, { once: true });
  }

  disconnectedCallback() {
    window.removeEventListener("unload", this.disconnectedCallback);
    Services.els.removeSystemEventListener(document, "keydown", this, false);
  }

  set handleCtrlTab(val) {
    this.setAttribute("handleCtrlTab", val);
    return val;
  }

  get handleCtrlTab() {
    return (this.getAttribute("handleCtrlTab") != "false");
  }
  /**
   * _tabs and _tabpanels are deprecated, they exist only for
   * backwards compatibility.
   */
  get _tabs() {
    return this.tabs;
  }

  get _tabpanels() {
    return this.tabpanels;
  }

  get tabs() {
    if (this.hasAttribute("tabcontainer")) {
      return document.getElementById(this.getAttribute("tabcontainer"));
    }
    return this.getElementsByTagNameNS(
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
      "tabs").item(0);
  }

  get tabpanels() {
    return this.getElementsByTagNameNS(
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
      "tabpanels").item(0);
  }

  set selectedIndex(val) {
    let tabs = this.tabs;
    if (tabs)
      tabs.selectedIndex = val;
    this.setAttribute("selectedIndex", val);
    return val;
  }

  get selectedIndex() {
    let tabs = this.tabs;
    return tabs ? tabs.selectedIndex : -1;
  }

  set selectedTab(val) {
    if (val) {
      let tabs = this.tabs;
      if (tabs)
        tabs.selectedItem = val;
    }
    return val;
  }

  get selectedTab() {
    let tabs = this.tabs;
    return tabs && tabs.selectedItem;
  }

  set selectedPanel(val) {
    if (val) {
      let tabpanels = this.tabpanels;
      if (tabpanels)
        tabpanels.selectedPanel = val;
    }
    return val;
  }

  get selectedPanel() {
    let tabpanels = this.tabpanels;
    return tabpanels && tabpanels.selectedPanel;
  }

  handleEvent(event) {
    if (!event.isTrusted) {
      // Don't let untrusted events mess with tabs.
      return;
    }

    // Don't check if the event was already consumed because tab
    // navigation should always work for better user experience.

    switch (event.keyCode) {
      case event.DOM_VK_TAB:
        if (event.ctrlKey && !event.altKey && !event.metaKey)
          if (this.tabs && this.handleCtrlTab) {
            this.tabs.advanceSelectedTab(event.shiftKey ? -1 : 1, true);
            event.preventDefault();
          }
        break;
      case event.DOM_VK_PAGE_UP:
        if (event.ctrlKey && !event.shiftKey && !event.altKey && !event.metaKey &&
          this.tabs) {
          this.tabs.advanceSelectedTab(-1, true);
          event.preventDefault();
        }
        break;
      case event.DOM_VK_PAGE_DOWN:
        if (event.ctrlKey && !event.shiftKey && !event.altKey && !event.metaKey &&
          this.tabs) {
          this.tabs.advanceSelectedTab(1, true);
          event.preventDefault();
        }
        break;
      case event.DOM_VK_LEFT:
        if (event.metaKey && event.altKey && !event.shiftKey && !event.ctrlKey)
          if (this.tabs && this._handleMetaAltArrows) {
            let offset = window.getComputedStyle(this)
              .direction == "ltr" ? -1 : 1;
            this.tabs.advanceSelectedTab(offset, true);
            event.preventDefault();
          }
        break;
      case event.DOM_VK_RIGHT:
        if (event.metaKey && event.altKey && !event.shiftKey && !event.ctrlKey)
          if (this.tabs && this._handleMetaAltArrows) {
            let offset = window.getComputedStyle(this)
              .direction == "ltr" ? 1 : -1;
            this.tabs.advanceSelectedTab(offset, true);
            event.preventDefault();
          }
        break;
    }
  }
}

customElements.define("tabbox", MozTabbox);

class MozTabpanels extends MozXULElement {
  connectedCallback() {
    if (this.delayConnectedCallback()) {
      return;
    }

    this._tabbox = null;
    this._selectedPanel = this.children.item(this.selectedIndex);
  }

  get tabbox() {
    // Memoize the result rather than replacing this getter, so that
    // it can be reset if the parent changes.
    if (this._tabbox) {
      return this._tabbox;
    }

    let parent = this.parentNode;
    while (parent) {
      if (parent.localName == "tabbox") {
        break;
      }
      parent = parent.parentNode;
    }

    return this._tabbox = parent;
  }

  set selectedIndex(val) {
    if (val < 0 || val >= this.children.length)
      return val;

    let panel = this._selectedPanel;
    this._selectedPanel = this.children[val];

    if (this.getAttribute("async") != "true") {
      this.setAttribute("selectedIndex", val);
    }

    if (this._selectedPanel != panel) {
      let event = document.createEvent("Events");
      event.initEvent("select", true, true);
      this.dispatchEvent(event);
    }
    return val;
  }

  get selectedIndex() {
    let indexStr = this.getAttribute("selectedIndex");
    return indexStr ? parseInt(indexStr) : -1;
  }

  set selectedPanel(val) {
    let selectedIndex = -1;
    for (let panel = val; panel != null; panel = panel.previousElementSibling)
      ++selectedIndex;
    this.selectedIndex = selectedIndex;
    return val;
  }

  get selectedPanel() {
    return this._selectedPanel;
  }

  /**
   * nsIDOMXULRelatedElement
   */
  getRelatedElement(aTabPanelElm) {
    if (!aTabPanelElm)
      return null;

    let tabboxElm = this.tabbox;
    if (!tabboxElm)
      return null;

    let tabsElm = tabboxElm.tabs;
    if (!tabsElm)
      return null;

    // Return tab element having 'linkedpanel' attribute equal to the id
    // of the tab panel or the same index as the tab panel element.
    let tabpanelIdx = Array.indexOf(this.children, aTabPanelElm);
    if (tabpanelIdx == -1)
      return null;

    let tabElms = tabsElm.children;
    let tabElmFromIndex = tabElms[tabpanelIdx];

    let tabpanelId = aTabPanelElm.id;
    if (tabpanelId) {
      for (let idx = 0; idx < tabElms.length; idx++) {
        let tabElm = tabElms[idx];
        if (tabElm.linkedPanel == tabpanelId)
          return tabElm;
      }
    }

    return tabElmFromIndex;
  }
}

MozXULElement.implementCustomInterface(MozTabpanels, [Ci.nsIDOMXULRelatedElement]);
customElements.define("tabpanels", MozTabpanels);

}
