/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  const MozXULMenuElement = MozElements.MozElementMixin(XULMenuElement);
  const MenuBaseControl = MozElements.BaseControlMixin(MozXULMenuElement);

  class MozMenuList extends MenuBaseControl {
    constructor() {
      super();

      this.addEventListener(
        "command",
        event => {
          if (event.target.parentNode.parentNode == this) {
            this.selectedItem = event.target;
          }
        },
        true
      );

      this.addEventListener("popupshowing", event => {
        if (event.target.parentNode == this) {
          this.activeChild = null;
          if (this.selectedItem) {
            // Not ready for auto-setting the active child in hierarchies yet.
            // For now, only do this when the outermost menupopup opens.
            this.activeChild = this.mSelectedInternal;
          }
        }
      });

      this.addEventListener(
        "keypress",
        event => {
          if (event.altKey || event.ctrlKey || event.metaKey) {
            return;
          }

          if (
            !event.defaultPrevented &&
            (event.keyCode == KeyEvent.DOM_VK_UP ||
              event.keyCode == KeyEvent.DOM_VK_DOWN ||
              event.keyCode == KeyEvent.DOM_VK_PAGE_UP ||
              event.keyCode == KeyEvent.DOM_VK_PAGE_DOWN ||
              event.keyCode == KeyEvent.DOM_VK_HOME ||
              event.keyCode == KeyEvent.DOM_VK_END ||
              event.keyCode == KeyEvent.DOM_VK_BACK_SPACE ||
              event.charCode > 0)
          ) {
            // Moving relative to an item: start from the currently selected item
            this.activeChild = this.mSelectedInternal;
            if (this.handleKeyPress(event)) {
              this.activeChild.doCommand();
              event.preventDefault();
            }
          }
        },
        { mozSystemGroup: true }
      );

      this.attachShadow({ mode: "open" });
    }

    static get inheritedAttributes() {
      return {
        image: "src=image",
        "#label": "value=label,crop,accesskey,highlightable",
        "#highlightable-label": "text=label,crop,accesskey,highlightable",
        dropmarker: "disabled,open",
      };
    }

    static get markup() {
      // Accessibility information of these nodes will be presented
      // on XULComboboxAccessible generated from <menulist>;
      // hide these nodes from the accessibility tree.
      return `
        <html:link href="chrome://global/skin/menulist.css" rel="stylesheet"/>
        <hbox id="label-box" part="label-box" flex="1" role="none">
          <image part="icon" role="none"/>
          <label id="label" part="label" crop="right" flex="1" role="none"/>
          <label id="highlightable-label" part="label" crop="right" flex="1" role="none"/>
        </hbox>
        <dropmarker part="dropmarker" exportparts="icon: dropmarker-icon" type="menu" role="none"/>
        <html:slot/>
    `;
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      if (!this.hasAttribute("popuponly")) {
        this.shadowRoot.appendChild(this.fragment);
        this._labelBox = this.shadowRoot.getElementById("label-box");
        this._dropmarker = this.shadowRoot.querySelector("dropmarker");
        this.initializeAttributeInheritance();
      } else {
        this.shadowRoot.appendChild(document.createElement("slot"));
      }

      this.mSelectedInternal = null;
      this.mAttributeObserver = null;
      this.setInitialSelection();
    }

    get fragment() {
      if (!this.constructor.hasOwnProperty("_fragment")) {
        this.constructor._fragment = MozXULElement.parseXULToFragment(
          MozMenuList.markup
        );
      }
      return document.importNode(this.constructor._fragment, true);
    }

    // nsIDOMXULSelectControlElement
    set value(val) {
      // if the new value is null, we still need to remove the old value
      if (val == null) {
        return (this.selectedItem = val);
      }

      var arr = null;
      var popup = this.menupopup;
      if (popup) {
        arr = popup.getElementsByAttribute("value", val);
      }

      if (arr && arr.item(0)) {
        this.selectedItem = arr[0];
      } else {
        this.selectedItem = null;
        this.setAttribute("value", val);
      }

      return val;
    }

    // nsIDOMXULSelectControlElement
    get value() {
      return this.getAttribute("value");
    }

    // nsIDOMXULMenuListElement
    set crop(val) {
      this.setAttribute("crop", val);
    }

    // nsIDOMXULMenuListElement
    get crop() {
      return this.getAttribute("crop");
    }

    // nsIDOMXULMenuListElement
    set image(val) {
      this.setAttribute("image", val);
      return val;
    }

    // nsIDOMXULMenuListElement
    get image() {
      return this.getAttribute("image");
    }

    // nsIDOMXULMenuListElement
    get label() {
      return this.getAttribute("label");
    }

    set description(val) {
      this.setAttribute("description", val);
      return val;
    }

    get description() {
      return this.getAttribute("description");
    }

    // nsIDOMXULMenuListElement
    set open(val) {
      this.openMenu(val);
      return val;
    }

    // nsIDOMXULMenuListElement
    get open() {
      return this.hasAttribute("open");
    }

    // nsIDOMXULSelectControlElement
    get itemCount() {
      return this.menupopup ? this.menupopup.children.length : 0;
    }

    get menupopup() {
      var popup = this.firstElementChild;
      while (popup && popup.localName != "menupopup") {
        popup = popup.nextElementSibling;
      }
      return popup;
    }

    // nsIDOMXULSelectControlElement
    set selectedIndex(val) {
      var popup = this.menupopup;
      if (popup && 0 <= val) {
        if (val < popup.children.length) {
          this.selectedItem = popup.children[val];
        }
      } else {
        this.selectedItem = null;
      }
      return val;
    }

    // nsIDOMXULSelectControlElement
    get selectedIndex() {
      // Quick and dirty. We won't deal with hierarchical menulists yet.
      if (
        !this.selectedItem ||
        !this.mSelectedInternal.parentNode ||
        this.mSelectedInternal.parentNode.parentNode != this
      ) {
        return -1;
      }

      var children = this.mSelectedInternal.parentNode.children;
      var i = children.length;
      while (i--) {
        if (children[i] == this.mSelectedInternal) {
          break;
        }
      }

      return i;
    }

    // nsIDOMXULSelectControlElement
    set selectedItem(val) {
      var oldval = this.mSelectedInternal;
      if (oldval == val) {
        return val;
      }

      if (val && !this.contains(val)) {
        return val;
      }

      if (oldval) {
        oldval.removeAttribute("selected");
        this.mAttributeObserver.disconnect();
      }

      this.mSelectedInternal = val;
      let attributeFilter = ["value", "label", "image", "description"];
      if (val) {
        val.setAttribute("selected", "true");
        for (let attr of attributeFilter) {
          if (val.hasAttribute(attr)) {
            this.setAttribute(attr, val.getAttribute(attr));
          } else {
            this.removeAttribute(attr);
          }
        }

        this.mAttributeObserver = new MutationObserver(
          this.handleMutation.bind(this)
        );
        this.mAttributeObserver.observe(val, { attributeFilter });
      } else {
        for (let attr of attributeFilter) {
          this.removeAttribute(attr);
        }
      }

      var event = document.createEvent("Events");
      event.initEvent("select", true, true);
      this.dispatchEvent(event);

      event = document.createEvent("Events");
      event.initEvent("ValueChange", true, true);
      this.dispatchEvent(event);

      return val;
    }

    // nsIDOMXULSelectControlElement
    get selectedItem() {
      return this.mSelectedInternal;
    }

    setInitialSelection() {
      var popup = this.menupopup;
      if (popup) {
        var arr = popup.getElementsByAttribute("selected", "true");

        var editable = this.editable;
        var value = this.value;
        if (!arr.item(0) && value) {
          arr = popup.getElementsByAttribute(
            editable ? "label" : "value",
            value
          );
        }

        if (arr.item(0)) {
          this.selectedItem = arr[0];
        } else if (!editable) {
          this.selectedIndex = 0;
        }
      }
    }

    contains(item) {
      if (!item) {
        return false;
      }

      var parent = item.parentNode;
      return parent && parent.parentNode == this;
    }

    handleMutation(aRecords) {
      for (let record of aRecords) {
        let t = record.target;
        if (t == this.mSelectedInternal) {
          let attrName = record.attributeName;
          switch (attrName) {
            case "value":
            case "label":
            case "image":
            case "description":
              if (t.hasAttribute(attrName)) {
                this.setAttribute(attrName, t.getAttribute(attrName));
              } else {
                this.removeAttribute(attrName);
              }
          }
        }
      }
    }

    // nsIDOMXULSelectControlElement
    getIndexOfItem(item) {
      var popup = this.menupopup;
      if (popup) {
        var children = popup.children;
        var i = children.length;
        while (i--) {
          if (children[i] == item) {
            return i;
          }
        }
      }
      return -1;
    }

    // nsIDOMXULSelectControlElement
    getItemAtIndex(index) {
      var popup = this.menupopup;
      if (popup) {
        var children = popup.children;
        if (index >= 0 && index < children.length) {
          return children[index];
        }
      }
      return null;
    }

    appendItem(label, value, description) {
      if (!this.menupopup) {
        this.appendChild(MozXULElement.parseXULToFragment(`<menupopup />`));
      }

      var popup = this.menupopup;
      popup.appendChild(MozXULElement.parseXULToFragment(`<menuitem />`));

      var item = popup.lastElementChild;
      if (label !== undefined) {
        item.setAttribute("label", label);
      }
      item.setAttribute("value", value);
      if (description) {
        item.setAttribute("description", description);
      }

      return item;
    }

    removeAllItems() {
      this.selectedItem = null;
      var popup = this.menupopup;
      if (popup) {
        this.removeChild(popup);
      }
    }

    disconnectedCallback() {
      if (this.mAttributeObserver) {
        this.mAttributeObserver.disconnect();
      }

      if (this._labelBox) {
        this._labelBox.remove();
        this._dropmarker.remove();
        this._labelBox = null;
        this._dropmarker = null;
      }
    }
  }

  MenuBaseControl.implementCustomInterface(MozMenuList, [
    Ci.nsIDOMXULMenuListElement,
    Ci.nsIDOMXULSelectControlElement,
  ]);

  customElements.define("menulist", MozMenuList);
}
