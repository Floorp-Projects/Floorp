/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

{
  class PanelList extends HTMLElement {
    static get observedAttributes() {
      return ["open"];
    }

    static get fragment() {
      if (!this._template) {
        let parser = new DOMParser();
        let doc = parser.parseFromString(
          `
          <template>
            <link rel="stylesheet" href="chrome://global/content/elements/panel-list.css">
            <div class="arrow top" role="presentation"></div>
            <div class="list" role="presentation">
              <slot></slot>
            </div>
            <div class="arrow bottom" role="presentation"></div>
          </template>
        `,
          "text/html"
        );
        this._template = document.importNode(
          doc.querySelector("template"),
          true
        );
      }
      let frag = this._template.content.cloneNode(true);
      if (window.IS_STORYBOOK) {
        frag.querySelector("link").href = "./panel-list/panel-list.css";
      }
      return frag;
    }

    constructor() {
      super();
      this.attachShadow({ mode: "open" });
      // Ensure that the element is hidden even if its main stylesheet hasn't
      // loaded yet. On initial load, or with cache disabled, the element could
      // briefly flicker before the stylesheet is loaded without this.
      let style = document.createElement("style");
      style.textContent = `
        :host(:not([open])) {
          display: none;
        }
      `;
      this.shadowRoot.appendChild(style);
      this.shadowRoot.appendChild(this.constructor.fragment);
    }

    connectedCallback() {
      this.setAttribute("role", "menu");
    }

    attributeChangedCallback(name, oldVal, newVal) {
      if (name == "open" && newVal != oldVal) {
        if (this.open) {
          this.onShow();
        } else {
          this.onHide();
        }
      }
    }

    get open() {
      return this.hasAttribute("open");
    }

    set open(val) {
      this.toggleAttribute("open", val);
    }

    getTargetForEvent(event) {
      if (!event) {
        return null;
      }
      if (event._savedComposedTarget) {
        return event._savedComposedTarget;
      }
      if (event.composed) {
        event._savedComposedTarget =
          event.composedTarget || event.composedPath()[0];
      }
      return event._savedComposedTarget || event.target;
    }

    show(triggeringEvent) {
      this.triggeringEvent = triggeringEvent;
      this.lastAnchorNode = this.getTargetForEvent(this.triggeringEvent);
      this.wasOpenedByKeyboard =
        triggeringEvent &&
        (triggeringEvent.inputSource == MouseEvent.MOZ_SOURCE_KEYBOARD ||
          triggeringEvent.inputSource == MouseEvent.MOZ_SOURCE_UNKNOWN);
      this.open = true;

      if (this.parentIsXULPanel()) {
        this.toggleAttribute("inxulpanel", true);
        let panel = this.parentElement;
        panel.hidden = false;
        // Bug 1842070 - There appears to be a race here where panel-lists
        // embedded in XUL panels won't appear during the first call to show()
        // without waiting for a mix of rAF and another tick of the event
        // loop.
        requestAnimationFrame(() => {
          setTimeout(() => {
            panel.openPopup(
              this.lastAnchorNode,
              "after_start",
              0,
              0,
              false,
              false,
              this.triggeringEvent
            );
          }, 0);
        });
      } else {
        this.toggleAttribute("inxulpanel", false);
      }
    }

    hide(triggeringEvent, { force = false } = {}) {
      // It's possible this is being used in an unprivileged context, in which
      // case it won't have access to Services / Services will be undeclared.
      const autohideDisabled = this.hasServices()
        ? Services.prefs.getBoolPref("ui.popup.disable_autohide", false)
        : false;

      if (autohideDisabled && !force) {
        // Don't hide if this wasn't "forced" (using escape or click in menu).
        return;
      }
      let openingEvent = this.triggeringEvent;
      this.triggeringEvent = triggeringEvent;
      this.open = false;

      if (this.parentIsXULPanel()) {
        // It's possible that we're being programattically hidden, in which
        // case, we need to hide the XUL panel we're embedded in. If, however,
        // we're being hidden because the XUL panel is being hidden, calling
        // hidePopup again on it is a no-op.
        let panel = this.parentElement;
        panel.hidePopup();
      }

      let target = this.getTargetForEvent(openingEvent);
      // Refocus the button that opened the menu if we have one.
      if (target && this.wasOpenedByKeyboard) {
        target.focus();
      }
    }

    toggle(triggeringEvent) {
      if (this.open) {
        this.hide(triggeringEvent, { force: true });
      } else {
        this.show(triggeringEvent);
      }
    }

    hasServices() {
      // Safely check for Services without throwing a ReferenceError.
      return typeof Services !== "undefined";
    }

    isDocumentRTL() {
      if (this.hasServices()) {
        return Services.locale.isAppLocaleRTL;
      }
      return document.dir === "rtl";
    }

    parentIsXULPanel() {
      const XUL_NS =
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      return (
        this.parentElement?.namespaceURI == XUL_NS &&
        this.parentElement?.localName == "panel"
      );
    }

    async setAlign() {
      const hostElement = this.parentElement || this.getRootNode().host;
      if (!hostElement) {
        // This could get called before we're added to the DOM.
        // Nothing to do in that case.
        return;
      }

      // Set the showing attribute to hide the panel until its alignment is set.
      this.setAttribute("showing", "true");
      // Tell the host element to hide any overflow in case the panel extends off
      // the page before the alignment is set.
      hostElement.style.overflow = "hidden";

      // Wait for a layout flush, then find the bounds.
      let {
        anchorHeight,
        anchorLeft,
        anchorTop,
        anchorWidth,
        panelHeight,
        panelWidth,
        winHeight,
        winScrollY,
        winScrollX,
        winWidth,
      } = await new Promise(resolve => {
        this.style.left = 0;
        this.style.top = 0;

        requestAnimationFrame(() =>
          setTimeout(() => {
            let target = this.getTargetForEvent(this.triggeringEvent);
            let anchorElement = target || hostElement;
            // It's possible this is being used in a context where windowUtils is
            // not available. In that case, fallback to using the element.
            let getBounds = el =>
              window.windowUtils
                ? window.windowUtils.getBoundsWithoutFlushing(el)
                : el.getBoundingClientRect();
            // Use y since top is reserved.
            let anchorBounds = getBounds(anchorElement);
            let panelBounds = getBounds(this);
            resolve({
              anchorHeight: anchorBounds.height,
              anchorLeft: anchorBounds.left,
              anchorTop: anchorBounds.top,
              anchorWidth: anchorBounds.width,
              panelHeight: panelBounds.height,
              panelWidth: panelBounds.width,
              winHeight: innerHeight,
              winWidth: innerWidth,
              winScrollX: scrollX,
              winScrollY: scrollY,
            });
          }, 0)
        );
      });

      // If we're embedded in a XUL panel, let it handle alignment.
      if (!this.parentIsXULPanel()) {
        // Calculate the left/right alignment.
        let align;
        let leftOffset;
        let leftAlignX = anchorLeft;
        let rightAlignX = anchorLeft + anchorWidth - panelWidth;

        if (this.isDocumentRTL()) {
          // Prefer aligning on the right.
          align = rightAlignX < 0 ? "left" : "right";
        } else {
          // Prefer aligning on the left.
          align = leftAlignX + panelWidth > winWidth ? "right" : "left";
        }
        leftOffset = align === "left" ? leftAlignX : rightAlignX;

        let bottomAlignY = anchorTop + anchorHeight;
        let valign;
        let topOffset;
        if (bottomAlignY + panelHeight > winHeight) {
          topOffset = anchorTop - panelHeight;
          valign = "top";
        } else {
          topOffset = bottomAlignY;
          valign = "bottom";
        }

        // Set the alignments and show the panel.
        this.setAttribute("align", align);
        this.setAttribute("valign", valign);
        hostElement.style.overflow = "";

        this.style.left = `${leftOffset + winScrollX}px`;
        this.style.top = `${topOffset + winScrollY}px`;
      }

      this.style.minWidth = this.hasAttribute("min-width-from-anchor")
        ? `${anchorWidth}px`
        : "";

      this.removeAttribute("showing");
    }

    addHideListeners() {
      if (this.hasAttribute("stay-open")) {
        // This is intended for inspection in Storybook.
        return;
      }
      // Hide when a panel-item is clicked in the list.
      this.addEventListener("click", this);
      document.addEventListener("keydown", this);
      // Hide when a click is initiated outside the panel.
      document.addEventListener("mousedown", this);
      // Hide if focus changes and the panel isn't in focus.
      document.addEventListener("focusin", this);
      // Reset or focus tracking, we treat the first focusin differently.
      this.focusHasChanged = false;
      // Hide on resize, scroll or losing window focus.
      window.addEventListener("resize", this);
      window.addEventListener("scroll", this, { capture: true });
      window.addEventListener("blur", this);
      if (this.parentIsXULPanel()) {
        this.parentElement.addEventListener("popuphidden", this);
      }
    }

    removeHideListeners() {
      this.removeEventListener("click", this);
      document.removeEventListener("keydown", this);
      document.removeEventListener("mousedown", this);
      document.removeEventListener("focusin", this);
      window.removeEventListener("resize", this);
      window.removeEventListener("scroll", this, { capture: true });
      window.removeEventListener("blur", this);
      if (this.parentIsXULPanel()) {
        this.parentElement.removeEventListener("popuphidden", this);
      }
    }

    handleEvent(e) {
      // Ignore the event if it caused the panel to open.
      if (e == this.triggeringEvent) {
        return;
      }

      let target = this.getTargetForEvent(e);
      let inPanelList = e.composed
        ? e.composedPath().some(el => el == this)
        : e.target.closest && e.target.closest("panel-list") == this;

      switch (e.type) {
        case "resize":
        case "scroll":
          if (inPanelList) {
            break;
          }
        // Intentional fall-through
        case "blur":
        case "popuphidden":
          this.hide();
          break;
        case "click":
          if (inPanelList) {
            this.hide(undefined, { force: true });
          } else {
            // Avoid falling through to the default click handler of the parent.
            e.stopPropagation();
          }
          break;
        case "mousedown":
          // Close if there's a click started outside the panel.
          if (!inPanelList) {
            this.hide();
          }
          break;
        case "keydown":
          if (e.key === "ArrowDown" || e.key === "ArrowUp" || e.key === "Tab") {
            // Ignore tabbing with a modifer other than shift.
            if (e.key === "Tab" && (e.altKey || e.ctrlKey || e.metaKey)) {
              return;
            }

            // Don't scroll the page or let the regular tab order take effect.
            e.preventDefault();

            // Keep moving to the next/previous element sibling until we find a
            // panel-item that isn't hidden.
            let moveForward =
              e.key === "ArrowDown" || (e.key === "Tab" && !e.shiftKey);

            // If the menu is opened with the mouse, the active element might be
            // somewhere else in the document. In that case we should ignore it
            // to avoid walking unrelated DOM nodes.
            this.focusWalker.currentNode = this.contains(
              this.getRootNode().activeElement
            )
              ? this.getRootNode().activeElement
              : this;
            let nextItem = moveForward
              ? this.focusWalker.nextNode()
              : this.focusWalker.previousNode();

            // If the next item wasn't found, try looping to the top/bottom.
            if (!nextItem) {
              this.focusWalker.currentNode = this;
              if (moveForward) {
                nextItem = this.focusWalker.firstChild();
              } else {
                nextItem = this.focusWalker.lastChild();
              }
            }
            break;
          } else if (e.key === "Escape") {
            this.hide(undefined, { force: true });
          } else if (!e.metaKey && !e.ctrlKey && !e.shiftKey && !e.altKey) {
            // Check if any of the children have an accesskey for this letter.
            let item = this.querySelector(
              `[accesskey="${e.key.toLowerCase()}"],
              [accesskey="${e.key.toUpperCase()}"]`
            );
            if (item) {
              item.click();
            }
          }
          break;
        case "focusin":
          if (
            this.triggeringEvent &&
            target == this.getTargetForEvent(this.triggeringEvent) &&
            !this.focusHasChanged
          ) {
            // There will be a focusin after the mousedown that opens the panel
            // using the mouse. Ignore the first focusin event if it's on the
            // triggering target.
            this.focusHasChanged = true;
          } else if (!target || !inPanelList) {
            // If the target isn't in the panel, hide. This will close when focus
            // moves out of the panel.
            this.hide();
          } else {
            // Just record that there was a focusin event.
            this.focusHasChanged = true;
          }
          break;
      }
    }

    /**
     * A TreeWalker that can be used to focus elements. The returned element will
     * be the element that has gained focus based on the requested movement
     * through the tree.
     *
     * Example:
     *
     *   this.focusWalker.currentNode = this;
     *   // Focus and get the first focusable child.
     *   let focused = this.focusWalker.nextNode();
     *   // Focus the second focusable child.
     *   this.focusWalker.nextNode();
     */
    get focusWalker() {
      if (!this._focusWalker) {
        this._focusWalker = document.createTreeWalker(
          this,
          NodeFilter.SHOW_ELEMENT,
          {
            acceptNode: node => {
              // No need to look at hidden nodes.
              if (node.hidden) {
                return NodeFilter.FILTER_REJECT;
              }

              // Focus the node, if it worked then this is the node we want.
              node.focus();
              if (node === node.getRootNode().activeElement) {
                return NodeFilter.FILTER_ACCEPT;
              }

              // Continue into child nodes if the parent couldn't be focused.
              return NodeFilter.FILTER_SKIP;
            },
          }
        );
      }
      return this._focusWalker;
    }

    async onShow() {
      this.sendEvent("showing");
      this.addHideListeners();
      await this.setAlign();

      // Wait until the next paint for the alignment to be set and panel to be
      // visible.
      requestAnimationFrame(() => {
        if (this.wasOpenedByKeyboard) {
          // Focus the first focusable panel-item if opened by keyboard.
          this.focusWalker.currentNode = this;
          this.focusWalker.nextNode();
        }

        this.lastAnchorNode?.setAttribute("aria-expanded", "true");

        this.sendEvent("shown");
      });
    }

    onHide() {
      requestAnimationFrame(() => {
        this.sendEvent("hidden");
        this.lastAnchorNode?.setAttribute("aria-expanded", "false");
      });
      this.removeHideListeners();
    }

    sendEvent(name, detail) {
      this.dispatchEvent(
        new CustomEvent(name, { detail, bubbles: true, composed: true })
      );
    }
  }
  customElements.define("panel-list", PanelList);

  class PanelItem extends HTMLElement {
    #initialized = false;
    #defaultSlot;

    static get observedAttributes() {
      return ["accesskey"];
    }

    constructor() {
      super();
      this.attachShadow({ mode: "open" });

      let style = document.createElement("link");
      style.rel = "stylesheet";
      style.href = window.IS_STORYBOOK
        ? "./panel-list/panel-item.css"
        : "chrome://global/content/elements/panel-item.css";

      this.button = document.createElement("button");
      this.button.setAttribute("role", "menuitem");
      this.button.setAttribute("part", "button");

      // Use a XUL label element if possible to show the accesskey.
      this.label = document.createXULElement
        ? document.createXULElement("label")
        : document.createElement("span");
      this.button.appendChild(this.label);

      let supportLinkSlot = document.createElement("slot");
      supportLinkSlot.name = "support-link";

      this.#defaultSlot = document.createElement("slot");
      this.#defaultSlot.style.display = "none";

      this.shadowRoot.append(
        style,
        this.button,
        supportLinkSlot,
        this.#defaultSlot
      );
    }

    connectedCallback() {
      if (!this.#initialized) {
        this.#initialized = true;
        // When click listeners are added to the panel-item it creates a node in
        // the a11y tree for this element. This breaks the association between the
        // menu and the button[role="menuitem"] in this shadow DOM and causes
        // announcement issues with screen readers. (bug 995064)
        this.setAttribute("role", "presentation");

        this.#setLabelContents();

        // When our content changes, move the text into the label. It doesn't work
        // with a <slot>, unfortunately.
        new MutationObserver(() => this.#setLabelContents()).observe(this, {
          characterData: true,
          childList: true,
          subtree: true,
        });
      }

      this.panel = this.closest("panel-list");

      if (this.panel) {
        this.panel.addEventListener("hidden", this);
        this.panel.addEventListener("shown", this);
      }
    }

    disconnectedCallback() {
      if (this.panel) {
        this.panel.removeEventListener("hidden", this);
        this.panel.removeEventListener("shown", this);
        this.panel = null;
      }
    }

    attributeChangedCallback(name, oldVal, newVal) {
      if (name === "accesskey") {
        // Bug 1037709 - Accesskey doesn't work in shadow DOM.
        // Ideally we'd have the accesskey set in shadow DOM, and on
        // attributeChangedCallback we'd just update the shadow DOM accesskey.

        // Skip this change event if we caused it.
        if (this._modifyingAccessKey) {
          this._modifyingAccessKey = false;
          return;
        }

        this.label.accessKey = newVal || "";

        // Bug 1588156 - Accesskey is not ignored for hidden non-input elements.
        // Since the accesskey won't be ignored, we need to remove it ourselves
        // when the panel is closed, and move it back when it opens.
        if (!this.panel || !this.panel.open) {
          // When the panel isn't open, just store the key for later.
          this._accessKey = newVal || null;
          this._modifyingAccessKey = true;
          this.accessKey = "";
        } else {
          this._accessKey = null;
        }
      }
    }

    #setLabelContents() {
      this.label.textContent = this.#defaultSlot
        .assignedNodes()
        .map(node => node.textContent)
        .join("");
    }

    get disabled() {
      return this.button.hasAttribute("disabled");
    }

    set disabled(val) {
      this.button.toggleAttribute("disabled", val);
    }

    get checked() {
      return this.hasAttribute("checked");
    }

    set checked(val) {
      this.toggleAttribute("checked", val);
    }

    focus() {
      this.button.focus();
    }

    handleEvent(e) {
      // Bug 1588156 - Accesskey is not ignored for hidden non-input elements.
      // Since the accesskey won't be ignored, we need to remove it ourselves
      // when the panel is closed, and move it back when it opens.
      switch (e.type) {
        case "shown":
          if (this._accessKey) {
            this.accessKey = this._accessKey;
            this._accessKey = null;
          }
          break;
        case "hidden":
          if (this.accessKey) {
            this._accessKey = this.accessKey;
            this._modifyingAccessKey = true;
            this.accessKey = "";
          }
          break;
      }
    }
  }
  customElements.define("panel-item", PanelItem);
}
