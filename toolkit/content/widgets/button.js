/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  class MozButtonBase extends MozElements.BaseText {
    constructor() {
      super();

      /**
       * While it would seem we could do this by handling oncommand, we can't
       * because any external oncommand handlers might get called before ours,
       * and then they would see the incorrect value of checked. Additionally
       * a command attribute would redirect the command events anyway.
       */
      this.addEventListener("click", event => {
        if (event.button != 0) {
          return;
        }
        this._handleClick();
      });

      this.addEventListener("keypress", event => {
        if (event.key != " ") {
          return;
        }
        this._handleClick();
        // Prevent page from scrolling on the space key.
        event.preventDefault();
      });

      this.addEventListener("keypress", event => {
        if (this.hasMenu()) {
          if (this.open) {
            return;
          }
        } else {
          if (
            event.keyCode == KeyEvent.DOM_VK_UP ||
            (event.keyCode == KeyEvent.DOM_VK_LEFT &&
              document.defaultView.getComputedStyle(this.parentNode)
                .direction == "ltr") ||
            (event.keyCode == KeyEvent.DOM_VK_RIGHT &&
              document.defaultView.getComputedStyle(this.parentNode)
                .direction == "rtl")
          ) {
            event.preventDefault();
            window.document.commandDispatcher.rewindFocus();
            return;
          }

          if (
            event.keyCode == KeyEvent.DOM_VK_DOWN ||
            (event.keyCode == KeyEvent.DOM_VK_RIGHT &&
              document.defaultView.getComputedStyle(this.parentNode)
                .direction == "ltr") ||
            (event.keyCode == KeyEvent.DOM_VK_LEFT &&
              document.defaultView.getComputedStyle(this.parentNode)
                .direction == "rtl")
          ) {
            event.preventDefault();
            window.document.commandDispatcher.advanceFocus();
            return;
          }
        }

        if (
          event.keyCode ||
          event.charCode <= 32 ||
          event.altKey ||
          event.ctrlKey ||
          event.metaKey
        ) {
          return;
        } // No printable char pressed, not a potential accesskey

        // Possible accesskey pressed
        var charPressedLower = String.fromCharCode(
          event.charCode
        ).toLowerCase();

        // If the accesskey of the current button is pressed, just activate it
        if (this.accessKey.toLowerCase() == charPressedLower) {
          this.click();
          return;
        }

        // Search for accesskey in the list of buttons for this doc and each subdoc
        // Get the buttons for the main document and all sub-frames
        for (
          var frameCount = -1;
          frameCount < window.top.frames.length;
          frameCount++
        ) {
          var doc =
            frameCount == -1
              ? window.top.document
              : window.top.frames[frameCount].document;
          if (this.fireAccessKeyButton(doc.documentElement, charPressedLower)) {
            return;
          }
        }

        // Test dialog buttons
        let buttonBox = window.top.document.documentElement.buttonBox;
        if (buttonBox) {
          this.fireAccessKeyButton(buttonBox, charPressedLower);
        }
      });
    }

    set type(val) {
      this.setAttribute("type", val);
      return val;
    }

    get type() {
      return this.getAttribute("type");
    }

    set disabled(val) {
      if (val) {
        this.setAttribute("disabled", "true");
      } else {
        this.removeAttribute("disabled");
      }
      return val;
    }

    get disabled() {
      return this.getAttribute("disabled") == "true";
    }

    set group(val) {
      this.setAttribute("group", val);
      return val;
    }

    get group() {
      return this.getAttribute("group");
    }

    set open(val) {
      if (this.hasMenu()) {
        this.openMenu(val);
      } else if (val) {
        // Fall back to just setting the attribute
        this.setAttribute("open", "true");
      } else {
        this.removeAttribute("open");
      }
      return val;
    }

    get open() {
      return this.hasAttribute("open");
    }

    set checked(val) {
      if (this.type == "radio" && val) {
        var sibs = this.parentNode.getElementsByAttribute("group", this.group);
        for (var i = 0; i < sibs.length; ++i) {
          sibs[i].removeAttribute("checked");
        }
      }

      if (val) {
        this.setAttribute("checked", "true");
      } else {
        this.removeAttribute("checked");
      }

      return val;
    }

    get checked() {
      return this.hasAttribute("checked");
    }

    filterButtons(node) {
      // if the node isn't visible, don't descend into it.
      var cs = node.ownerGlobal.getComputedStyle(node);
      if (cs.visibility != "visible" || cs.display == "none") {
        return NodeFilter.FILTER_REJECT;
      }
      // but it may be a popup element, in which case we look at "state"...
      if (cs.display == "-moz-popup" && node.state != "open") {
        return NodeFilter.FILTER_REJECT;
      }
      // OK - the node seems visible, so it is a candidate.
      if (node.localName == "button" && node.accessKey && !node.disabled) {
        return NodeFilter.FILTER_ACCEPT;
      }
      return NodeFilter.FILTER_SKIP;
    }

    fireAccessKeyButton(aSubtree, aAccessKeyLower) {
      var iterator = aSubtree.ownerDocument.createTreeWalker(
        aSubtree,
        NodeFilter.SHOW_ELEMENT,
        this.filterButtons
      );
      while (iterator.nextNode()) {
        var test = iterator.currentNode;
        if (
          test.accessKey.toLowerCase() == aAccessKeyLower &&
          !test.disabled &&
          !test.collapsed &&
          !test.hidden
        ) {
          test.focus();
          test.click();
          return true;
        }
      }
      return false;
    }

    _handleClick() {
      if (!this.disabled) {
        if (this.type == "checkbox") {
          this.checked = !this.checked;
        } else if (this.type == "radio") {
          this.checked = true;
        }
      }
    }
  }

  MozXULElement.implementCustomInterface(MozButtonBase, [
    Ci.nsIDOMXULButtonElement,
  ]);

  MozElements.ButtonBase = MozButtonBase;

  class MozButton extends MozButtonBase {
    static get inheritedAttributes() {
      return {
        ".box-inherit": "align,dir,pack,orient",
        ".button-icon": "src=image",
        ".button-text": "value=label,accesskey,crop",
        ".button-menu-dropmarker": "open,disabled,label",
      };
    }

    get icon() {
      return this.querySelector(".button-icon");
    }

    static get buttonFragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
        <hbox class="box-inherit button-box" align="center" pack="center" flex="1" anonid="button-box">
          <image class="button-icon"/>
          <label class="button-text"/>
        </hbox>`),
        true
      );
      Object.defineProperty(this, "buttonFragment", { value: frag });
      return frag;
    }

    static get menuFragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
        <hbox class="box-inherit button-box" align="center" pack="center" flex="1">
          <hbox class="box-inherit" align="center" pack="center" flex="1">
            <image class="button-icon"/>
            <label class="button-text"/>
          </hbox>
          <dropmarker class="button-menu-dropmarker"/>
        </hbox>`),
        true
      );
      Object.defineProperty(this, "menuFragment", { value: frag });
      return frag;
    }

    get _hasConnected() {
      return this.querySelector(":scope > .button-box") != null;
    }

    connectedCallback() {
      if (this.delayConnectedCallback() || this._hasConnected) {
        return;
      }

      let fragment;
      if (this.type === "menu") {
        fragment = MozButton.menuFragment;

        this.addEventListener("keypress", event => {
          if (event.keyCode != KeyEvent.DOM_VK_RETURN && event.key != " ") {
            return;
          }

          this.open = true;
          // Prevent page from scrolling on the space key.
          if (event.key == " ") {
            event.preventDefault();
          }
        });
      } else {
        fragment = this.constructor.buttonFragment;
      }

      this.appendChild(fragment.cloneNode(true));
      this.initializeAttributeInheritance();
    }
  }

  customElements.define("button", MozButton);
}
