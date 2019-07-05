/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  const KEEP_CHILDREN = new Set([
    "observes",
    "template",
    "menupopup",
    "panel",
    "tooltip",
  ]);

  window.addEventListener(
    "popupshowing",
    e => {
      if (e.originalTarget.ownerDocument != document) {
        return;
      }

      e.originalTarget.setAttribute("hasbeenopened", "true");
      for (let el of e.originalTarget.querySelectorAll("toolbarbutton")) {
        el.render();
      }
    },
    { capture: true }
  );

  class MozToolbarbutton extends MozElements.ButtonBase {
    static get inheritedAttributes() {
      return {
        ".toolbarbutton-icon":
          "validate,src=image,label,type,consumeanchor,triggeringprincipal=iconloadingprincipal",
        ".toolbarbutton-text": "value=label,accesskey,crop,dragover-top,wrap",
        ".toolbarbutton-multiline-text": "text=label,accesskey,wrap",
        ".toolbarbutton-menu-dropmarker": "disabled,label",

        ".toolbarbutton-badge": "value=badge,style=badgeStyle",
      };
    }

    static get fragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
        <image class="toolbarbutton-icon"></image>
        <label class="toolbarbutton-text" crop="right" flex="1"></label>
        <label class="toolbarbutton-multiline-text" flex="1"></label>
        <dropmarker type="menu" class="toolbarbutton-menu-dropmarker"></dropmarker>`),
        true
      );
      Object.defineProperty(this, "fragment", { value: frag });
      return frag;
    }

    static get badgedFragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
        <stack class="toolbarbutton-badge-stack">
          <image class="toolbarbutton-icon"/>
          <label class="toolbarbutton-badge" top="0" end="0" crop="none"/>
        </stack>
        <label class="toolbarbutton-text" crop="right" flex="1"/>
        <label class="toolbarbutton-multiline-text" flex="1"/>
        <dropmarker anonid="dropmarker" type="menu"
                    class="toolbarbutton-menu-dropmarker"/>`),
        true
      );
      Object.defineProperty(this, "badgedFragment", { value: frag });
      return frag;
    }

    get _hasRendered() {
      return this.querySelector(":scope > .toolbarbutton-text") != null;
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      // Defer creating DOM elements for content inside popups.
      // These will be added in the popupshown handler above.
      let panel = this.closest("panel");
      if (panel && !panel.hasAttribute("hasbeenopened")) {
        return;
      }

      this.render();
    }

    render() {
      if (this._hasRendered) {
        return;
      }

      let badged = this.getAttribute("badged") == "true";

      if (badged) {
        let moveChildren = [];
        for (let child of this.children) {
          if (!KEEP_CHILDREN.has(child.tagName)) {
            moveChildren.push(child);
          }
        }

        this.appendChild(this.constructor.badgedFragment.cloneNode(true));

        if (moveChildren.length > 0) {
          let { badgeStack, icon } = this;
          for (let child of moveChildren) {
            badgeStack.insertBefore(child, icon);
          }
        }
      } else {
        let moveChildren = [];
        for (let child of this.children) {
          if (!KEEP_CHILDREN.has(child.tagName) && child.tagName != "box") {
            // XBL toolbarbutton doesn't insert any anonymous content
            // if it has a child of any other type
            return;
          }

          if (child.tagName == "box") {
            moveChildren.push(child);
          }
        }

        this.appendChild(this.constructor.fragment.cloneNode(true));

        // XBL toolbarbutton explicitly places any <box> children
        // right before the menu marker.
        for (let child of moveChildren) {
          this.insertBefore(child, this.lastChild);
        }
      }

      this.initializeAttributeInheritance();
    }

    get icon() {
      return this.querySelector(".toolbarbutton-icon");
    }

    get badgeLabel() {
      return this.querySelector(".toolbarbutton-badge");
    }

    get badgeStack() {
      return this.querySelector(".toolbarbutton-badge-stack");
    }

    get multilineLabel() {
      return this.querySelector(".toolbarbutton-multiline-text");
    }

    get dropmarker() {
      return this.querySelector(".toolbarbutton-menu-dropmarker");
    }

    get menupopup() {
      return this.querySelector("menupopup");
    }
  }

  customElements.define("toolbarbutton", MozToolbarbutton);
}
