/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

{

class MozTabbox extends MozXULElement {
  constructor() {
    super();
    this._handleMetaAltArrows = /Mac/.test(navigator.platform);
    this.disconnectedCallback = this.disconnectedCallback.bind(this);
  }

  connectedCallback() {
    switch (this.getAttribute("eventnode")) {
      case "parent":
        this._eventNode = this.parentNode;
        break;
      case "window":
        this._eventNode = window;
        break;
      case "document":
        this._eventNode = document;
        break;
      default:
        this._eventNode = this;
    }

    Services.els.addSystemEventListener(this._eventNode, "keydown", this, false);
    window.addEventListener("unload", this.disconnectedCallback, { once: true });
  }

  disconnectedCallback() {
    window.removeEventListener("unload", this.disconnectedCallback);
    Services.els.removeSystemEventListener(this._eventNode, "keydown", this, false);
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
    var tabs = this.tabs;
    if (tabs)
      tabs.selectedIndex = val;
    this.setAttribute("selectedIndex", val);
    return val;
  }

  get selectedIndex() {
    var tabs = this.tabs;
    return tabs ? tabs.selectedIndex : -1;
  }

  set selectedTab(val) {
    if (val) {
      var tabs = this.tabs;
      if (tabs)
        tabs.selectedItem = val;
    }
    return val;
  }

  get selectedTab() {
    var tabs = this.tabs;
    return tabs && tabs.selectedItem;
  }

  set selectedPanel(val) {
    if (val) {
      var tabpanels = this.tabpanels;
      if (tabpanels)
        tabpanels.selectedPanel = val;
    }
    return val;
  }

  get selectedPanel() {
    var tabpanels = this.tabpanels;
    return tabpanels && tabpanels.selectedPanel;
  }

  set eventNode(val) {
    if (val != this._eventNode) {
      Services.els.addSystemEventListener(val, "keydown", this, false);
      Services.els.removeSystemEventListener(this._eventNode, "keydown", this, false);
      this._eventNode = val;
    }
    return val;
  }

  get eventNode() {
    return this._eventNode;
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
            var offset = window.getComputedStyle(this)
              .direction == "ltr" ? -1 : 1;
            this.tabs.advanceSelectedTab(offset, true);
            event.preventDefault();
          }
        break;
      case event.DOM_VK_RIGHT:
        if (event.metaKey && event.altKey && !event.shiftKey && !event.ctrlKey)
          if (this.tabs && this._handleMetaAltArrows) {
            offset = window.getComputedStyle(this)
              .direction == "ltr" ? 1 : -1;
            this.tabs.advanceSelectedTab(offset, true);
            event.preventDefault();
          }
        break;
    }
  }
}

customElements.define("tabbox", MozTabbox);

}
