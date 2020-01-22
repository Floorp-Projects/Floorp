/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );
  const { AppConstants } = ChromeUtils.import(
    "resource://gre/modules/AppConstants.jsm"
  );

  let imports = {};
  ChromeUtils.defineModuleGetter(
    imports,
    "ShortcutUtils",
    "resource://gre/modules/ShortcutUtils.jsm"
  );

  class MozTabbox extends MozXULElement {
    constructor() {
      super();
      this._handleMetaAltArrows = AppConstants.platform == "macosx";
      this.disconnectedCallback = this.disconnectedCallback.bind(this);
    }

    connectedCallback() {
      Services.els.addSystemEventListener(document, "keydown", this, false);
      window.addEventListener("unload", this.disconnectedCallback, {
        once: true,
      });
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
      return this.getAttribute("handleCtrlTab") != "false";
    }

    get tabs() {
      if (this.hasAttribute("tabcontainer")) {
        return document.getElementById(this.getAttribute("tabcontainer"));
      }
      return this.getElementsByTagNameNS(
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
        "tabs"
      ).item(0);
    }

    get tabpanels() {
      return this.getElementsByTagNameNS(
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
        "tabpanels"
      ).item(0);
    }

    set selectedIndex(val) {
      let tabs = this.tabs;
      if (tabs) {
        tabs.selectedIndex = val;
      }
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
        if (tabs) {
          tabs.selectedItem = val;
        }
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
        if (tabpanels) {
          tabpanels.selectedPanel = val;
        }
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

      // Skip this only if something has explicitly cancelled it.
      if (event.defaultCancelled) {
        return;
      }

      // Don't check if the event was already consumed because tab
      // navigation should always work for better user experience.

      const { ShortcutUtils } = imports;

      switch (ShortcutUtils.getSystemActionForEvent(event)) {
        case ShortcutUtils.CYCLE_TABS:
          if (this.tabs && this.handleCtrlTab) {
            this.tabs.advanceSelectedTab(event.shiftKey ? -1 : 1, true);
            event.preventDefault();
          }
          break;
        case ShortcutUtils.PREVIOUS_TAB:
          if (this.tabs) {
            this.tabs.advanceSelectedTab(-1, true);
            event.preventDefault();
          }
          break;
        case ShortcutUtils.NEXT_TAB:
          if (this.tabs) {
            this.tabs.advanceSelectedTab(1, true);
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

      return (this._tabbox = parent);
    }

    set selectedIndex(val) {
      if (val < 0 || val >= this.children.length) {
        return val;
      }

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
      for (
        let panel = val;
        panel != null;
        panel = panel.previousElementSibling
      ) {
        ++selectedIndex;
      }
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
      if (!aTabPanelElm) {
        return null;
      }

      let tabboxElm = this.tabbox;
      if (!tabboxElm) {
        return null;
      }

      let tabsElm = tabboxElm.tabs;
      if (!tabsElm) {
        return null;
      }

      // Return tab element having 'linkedpanel' attribute equal to the id
      // of the tab panel or the same index as the tab panel element.
      let tabpanelIdx = Array.prototype.indexOf.call(
        this.children,
        aTabPanelElm
      );
      if (tabpanelIdx == -1) {
        return null;
      }

      let tabElms = tabsElm.allTabs;
      let tabElmFromIndex = tabElms[tabpanelIdx];

      let tabpanelId = aTabPanelElm.id;
      if (tabpanelId) {
        for (let idx = 0; idx < tabElms.length; idx++) {
          let tabElm = tabElms[idx];
          if (tabElm.linkedPanel == tabpanelId) {
            return tabElm;
          }
        }
      }

      return tabElmFromIndex;
    }
  }

  MozXULElement.implementCustomInterface(MozTabpanels, [
    Ci.nsIDOMXULRelatedElement,
  ]);
  customElements.define("tabpanels", MozTabpanels);

  MozElements.MozTab = class MozTab extends MozElements.BaseText {
    constructor() {
      super();

      this.addEventListener("mousedown", this);
      this.addEventListener("keydown", this);

      this.arrowKeysShouldWrap = AppConstants.platform == "macosx";
    }

    static get inheritedAttributes() {
      return {
        ".tab-middle": "align,dir,pack,orient,selected,visuallyselected",
        ".tab-icon": "validate,src=image",
        ".tab-text": "value=label,accesskey,crop,disabled",
      };
    }

    get fragment() {
      if (!this._fragment) {
        this._fragment = MozXULElement.parseXULToFragment(`
        <hbox class="tab-middle box-inherit" flex="1">
          <image class="tab-icon" role="presentation"></image>
          <label class="tab-text" flex="1" role="presentation"></label>
        </hbox>
    `);
      }
      return this.ownerDocument.importNode(this._fragment, true);
    }

    connectedCallback() {
      if (!this._initialized) {
        this.textContent = "";
        this.appendChild(this.fragment);
        this.initializeAttributeInheritance();
        this._initialized = true;
      }
    }

    on_mousedown(event) {
      if (event.button != 0 || this.disabled) {
        return;
      }

      this.parentNode.ariaFocusedItem = null;

      if (this == this.parentNode.selectedItem) {
        // This tab is already selected and we will fall
        // through to mousedown behavior which sets focus on the current tab,
        // Only a click on an already selected tab should focus the tab itself.
        return;
      }

      let stopwatchid = this.parentNode.getAttribute("stopwatchid");
      if (stopwatchid) {
        TelemetryStopwatch.start(stopwatchid);
      }

      // Call this before setting the 'ignorefocus' attribute because this
      // will pass on focus if the formerly selected tab was focused as well.
      this.closest("tabs")._selectNewTab(this);

      var isTabFocused = false;
      try {
        isTabFocused = document.commandDispatcher.focusedElement == this;
      } catch (e) {}

      // Set '-moz-user-focus' to 'ignore' so that PostHandleEvent() can't
      // focus the tab; we only want tabs to be focusable by the mouse if
      // they are already focused. After a short timeout we'll reset
      // '-moz-user-focus' so that tabs can be focused by keyboard again.
      if (!isTabFocused) {
        this.setAttribute("ignorefocus", "true");
        setTimeout(tab => tab.removeAttribute("ignorefocus"), 0, this);
      }

      if (stopwatchid) {
        TelemetryStopwatch.finish(stopwatchid);
      }
    }

    on_keydown(event) {
      if (event.ctrlKey || event.altKey || event.metaKey || event.shiftKey) {
        return;
      }
      switch (event.keyCode) {
        case KeyEvent.DOM_VK_LEFT: {
          let direction = window.getComputedStyle(this.parentNode).direction;
          this.container.advanceSelectedTab(
            direction == "ltr" ? -1 : 1,
            this.arrowKeysShouldWrap
          );
          event.preventDefault();
          break;
        }

        case KeyEvent.DOM_VK_RIGHT: {
          let direction = window.getComputedStyle(this.parentNode).direction;
          this.container.advanceSelectedTab(
            direction == "ltr" ? 1 : -1,
            this.arrowKeysShouldWrap
          );
          event.preventDefault();
          break;
        }

        case KeyEvent.DOM_VK_UP:
          this.container.advanceSelectedTab(-1, this.arrowKeysShouldWrap);
          event.preventDefault();
          break;

        case KeyEvent.DOM_VK_DOWN:
          this.container.advanceSelectedTab(1, this.arrowKeysShouldWrap);
          event.preventDefault();
          break;

        case KeyEvent.DOM_VK_HOME:
          this.container._selectNewTab(this.container.allTabs[0]);
          event.preventDefault();
          break;

        case KeyEvent.DOM_VK_END: {
          let { allTabs } = this.container;
          this.container._selectNewTab(allTabs[allTabs.length - 1], -1);
          event.preventDefault();
          break;
        }
      }
    }

    set value(val) {
      this.setAttribute("value", val);
      return val;
    }

    get value() {
      return this.getAttribute("value");
    }

    get control() {
      var parent = this.parentNode;
      return parent.localName == "tabs" ? parent : null;
    }

    get selected() {
      return this.getAttribute("selected") == "true";
    }

    set _selected(val) {
      if (val) {
        this.setAttribute("selected", "true");
        this.setAttribute("visuallyselected", "true");
      } else {
        this.removeAttribute("selected");
        this.removeAttribute("visuallyselected");
      }

      return val;
    }

    set linkedPanel(val) {
      this.setAttribute("linkedpanel", val);
    }

    get linkedPanel() {
      return this.getAttribute("linkedpanel");
    }
  };

  MozXULElement.implementCustomInterface(MozElements.MozTab, [
    Ci.nsIDOMXULSelectControlItemElement,
  ]);
  customElements.define("tab", MozElements.MozTab);

  class TabsBase extends MozElements.BaseControl {
    constructor() {
      super();

      this.addEventListener("DOMMouseScroll", event => {
        if (Services.prefs.getBoolPref("toolkit.tabbox.switchByScrolling")) {
          if (event.detail > 0) {
            this.advanceSelectedTab(1, false);
          } else {
            this.advanceSelectedTab(-1, false);
          }
          event.stopPropagation();
        }
      });
    }

    // to be called from derived class connectedCallback
    baseConnect() {
      this._tabbox = null;
      this.ACTIVE_DESCENDANT_ID =
        "keyboard-focused-tab-" + Math.trunc(Math.random() * 1000000);

      if (!this.hasAttribute("orient")) {
        this.setAttribute("orient", "horizontal");
      }

      if (this.tabbox && this.tabbox.hasAttribute("selectedIndex")) {
        let selectedIndex = parseInt(this.tabbox.getAttribute("selectedIndex"));
        this.selectedIndex = selectedIndex > 0 ? selectedIndex : 0;
        return;
      }

      let children = this.allTabs;
      let length = children.length;
      for (var i = 0; i < length; i++) {
        if (children[i].getAttribute("selected") == "true") {
          this.selectedIndex = i;
          return;
        }
      }

      var value = this.value;
      if (value) {
        this.value = value;
      } else {
        this.selectedIndex = 0;
      }
    }

    /**
     * nsIDOMXULSelectControlElement
     */
    get itemCount() {
      return this.allTabs.length;
    }

    set value(val) {
      this.setAttribute("value", val);
      var children = this.allTabs;
      for (var c = children.length - 1; c >= 0; c--) {
        if (children[c].value == val) {
          this.selectedIndex = c;
          break;
        }
      }
      return val;
    }

    get value() {
      return this.getAttribute("value");
    }

    get tabbox() {
      if (!this._tabbox) {
        // Memoize the result in a field rather than replacing this property,
        // so that it can be reset along with the binding.
        this._tabbox = this.closest("tabbox");
      }

      return this._tabbox;
    }

    set selectedIndex(val) {
      var tab = this.getItemAtIndex(val);
      if (tab) {
        for (let otherTab of this.allTabs) {
          if (otherTab != tab && otherTab.selected) {
            otherTab._selected = false;
          }
        }
        tab._selected = true;

        this.setAttribute("value", tab.value);

        let linkedPanel = this.getRelatedElement(tab);
        if (linkedPanel) {
          this.tabbox.setAttribute("selectedIndex", val);

          // This will cause an onselect event to fire for the tabpanel
          // element.
          this.tabbox.tabpanels.selectedPanel = linkedPanel;
        }
      }
      return val;
    }

    get selectedIndex() {
      const tabs = this.allTabs;
      for (var i = 0; i < tabs.length; i++) {
        if (tabs[i].selected) {
          return i;
        }
      }
      return -1;
    }

    set selectedItem(val) {
      if (val && !val.selected) {
        // The selectedIndex setter ignores invalid values
        // such as -1 if |val| isn't one of our child nodes.
        this.selectedIndex = this.getIndexOfItem(val);
      }
      return val;
    }

    get selectedItem() {
      const tabs = this.allTabs;
      for (var i = 0; i < tabs.length; i++) {
        if (tabs[i].selected) {
          return tabs[i];
        }
      }
      return null;
    }

    get ariaFocusedIndex() {
      const tabs = this.allTabs;
      for (var i = 0; i < tabs.length; i++) {
        if (tabs[i].id == this.ACTIVE_DESCENDANT_ID) {
          return i;
        }
      }
      return -1;
    }

    set ariaFocusedItem(val) {
      let setNewItem = val && this.getIndexOfItem(val) != -1;
      let clearExistingItem = this.ariaFocusedItem && (!val || setNewItem);
      if (clearExistingItem) {
        let ariaFocusedItem = this.ariaFocusedItem;
        ariaFocusedItem.classList.remove("keyboard-focused-tab");
        ariaFocusedItem.id = "";
        this.selectedItem.removeAttribute("aria-activedescendant");
        let evt = new CustomEvent("AriaFocus");
        this.selectedItem.dispatchEvent(evt);
      }

      if (setNewItem) {
        this.ariaFocusedItem = null;
        val.id = this.ACTIVE_DESCENDANT_ID;
        val.classList.add("keyboard-focused-tab");
        this.selectedItem.setAttribute(
          "aria-activedescendant",
          this.ACTIVE_DESCENDANT_ID
        );
        let evt = new CustomEvent("AriaFocus");
        val.dispatchEvent(evt);
      }

      return val;
    }

    get ariaFocusedItem() {
      return document.getElementById(this.ACTIVE_DESCENDANT_ID);
    }

    /**
     * nsIDOMXULRelatedElement
     */
    getRelatedElement(aTabElm) {
      if (!aTabElm) {
        return null;
      }

      let tabboxElm = this.tabbox;
      if (!tabboxElm) {
        return null;
      }

      let tabpanelsElm = tabboxElm.tabpanels;
      if (!tabpanelsElm) {
        return null;
      }

      // Get linked tab panel by 'linkedpanel' attribute on the given tab
      // element.
      let linkedPanelId = aTabElm.linkedPanel;
      if (linkedPanelId) {
        return this.ownerDocument.getElementById(linkedPanelId);
      }

      // otherwise linked tabpanel element has the same index as the given
      // tab element.
      let tabElmIdx = this.getIndexOfItem(aTabElm);
      return tabpanelsElm.children[tabElmIdx];
    }

    getIndexOfItem(item) {
      return Array.prototype.indexOf.call(this.allTabs, item);
    }

    getItemAtIndex(index) {
      return this.allTabs[index] || null;
    }

    /**
     * Find an adjacent tab.
     *
     * @param {Node} startTab         A <tab> element to start searching from.
     * @param {Number} opts.direction 1 to search forward, -1 to search backward.
     * @param {Boolean} opts.wrap     If true, wrap around if the search reaches
     *                                the end (or beginning) of the tab strip.
     * @param {Boolean} opts.startWithAdjacent
     *                                If true (which is the default), start
     *                                searching from the  next tab after (or
     *                                before) startTab.  If false, startTab may
     *                                be returned if it passes the filter.
     * @param {Boolean} opts.advance  If false, start searching with startTab.  If
     *                                true, start searching with an adjacent tab.
     * @param {Function} opts.filter  A function to select which tabs to return.
     *
     * @return {Node | null}     The next <tab> element or, if none exists, null.
     */
    findNextTab(startTab, opts = {}) {
      let {
        direction = 1,
        wrap = false,
        startWithAdjacent = true,
        filter = tab => true,
      } = opts;

      let tab = startTab;
      if (!startWithAdjacent && filter(tab)) {
        return tab;
      }

      let children = this.allTabs;
      let i = children.indexOf(tab);
      if (i < 0) {
        return null;
      }

      while (true) {
        i += direction;
        if (wrap) {
          if (i < 0) {
            i = children.length - 1;
          } else if (i >= children.length) {
            i = 0;
          }
        } else if (i < 0 || i >= children.length) {
          return null;
        }

        tab = children[i];
        if (tab == startTab) {
          return null;
        }
        if (filter(tab)) {
          return tab;
        }
      }
    }

    _selectNewTab(aNewTab, aFallbackDir, aWrap) {
      this.ariaFocusedItem = null;

      aNewTab = this.findNextTab(aNewTab, {
        direction: aFallbackDir,
        wrap: aWrap,
        startWithAdjacent: false,
        filter: tab =>
          !tab.hidden && !tab.disabled && this._canAdvanceToTab(tab),
      });

      var isTabFocused = false;
      try {
        isTabFocused =
          document.commandDispatcher.focusedElement == this.selectedItem;
      } catch (e) {}
      this.selectedItem = aNewTab;
      if (isTabFocused) {
        aNewTab.focus();
      } else if (this.getAttribute("setfocus") != "false") {
        let selectedPanel = this.tabbox.selectedPanel;
        document.commandDispatcher.advanceFocusIntoSubtree(selectedPanel);

        // Make sure that the focus doesn't move outside the tabbox
        if (this.tabbox) {
          try {
            let el = document.commandDispatcher.focusedElement;
            while (el && el != this.tabbox.tabpanels) {
              if (el == this.tabbox || el == selectedPanel) {
                return;
              }
              el = el.parentNode;
            }
            aNewTab.focus();
          } catch (e) {}
        }
      }
    }

    _canAdvanceToTab(aTab) {
      return true;
    }

    advanceSelectedTab(aDir, aWrap) {
      let startTab = this.ariaFocusedItem || this.selectedItem;
      let newTab = this.findNextTab(startTab, {
        direction: aDir,
        wrap: aWrap,
      });
      if (newTab && newTab != startTab) {
        this._selectNewTab(newTab, aDir, aWrap);
      }
    }

    appendItem(label, value) {
      var tab = document.createXULElement("tab");
      tab.setAttribute("label", label);
      tab.setAttribute("value", value);
      this.appendChild(tab);
      return tab;
    }
  }

  MozXULElement.implementCustomInterface(TabsBase, [
    Ci.nsIDOMXULSelectControlElement,
    Ci.nsIDOMXULRelatedElement,
  ]);

  MozElements.TabsBase = TabsBase;

  class MozTabs extends TabsBase {
    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      let start = MozXULElement.parseXULToFragment(
        `<spacer class="tabs-left"/>`
      );
      this.insertBefore(start, this.firstChild);

      let end = MozXULElement.parseXULToFragment(
        `<spacer class="tabs-right" flex="1"/>`
      );
      this.insertBefore(end, null);

      this.baseConnect();
    }

    // Accessor for tabs.  This element has spacers as the first and
    // last elements and <tab>s are everything in between.
    get allTabs() {
      let children = Array.from(this.children);
      return children.splice(1, children.length - 2);
    }

    appendChild(tab) {
      // insert before the end spacer.
      this.insertBefore(tab, this.lastChild);
    }
  }

  customElements.define("tabs", MozTabs);
}
