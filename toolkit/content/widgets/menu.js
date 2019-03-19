/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
class MozMenuItemBase extends MozElements.BaseText {
  // nsIDOMXULSelectControlItemElement
  set value(val) {
    this.setAttribute("value", val);
  }
  get value() {
    return this.getAttribute("value");
  }

  // nsIDOMXULSelectControlItemElement
  get selected() {
    return this.getAttribute("selected") == "true";
  }

  // nsIDOMXULSelectControlItemElement
  get control() {
    var parent = this.parentNode;
    // Return the parent if it is a menu or menulist.
    if (parent && parent.parentNode instanceof XULMenuElement) {
      return parent.parentNode;
    }
    return null;
  }

  // nsIDOMXULContainerItemElement
  get parentContainer() {
    for (var parent = this.parentNode; parent; parent = parent.parentNode) {
      if (parent instanceof XULMenuElement) {
        return parent;
      }
    }
    return null;
  }
}

MozXULElement.implementCustomInterface(MozMenuItemBase, [Ci.nsIDOMXULSelectControlItemElement, Ci.nsIDOMXULContainerItemElement]);

class MozMenuBase extends MozMenuItemBase {
  set open(val) {
    this.openMenu(val);
    return val;
  }

  get open() {
    return this.hasAttribute("open");
  }

  get itemCount() {
    var menupopup = this.menupopup;
    return menupopup ? menupopup.children.length : 0;
  }

  get menupopup() {
    const XUL_NS =
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    for (var child = this.firstElementChild; child; child = child.nextElementSibling) {
      if (child.namespaceURI == XUL_NS && child.localName == "menupopup")
        return child;
    }
    return null;
  }

  appendItem(aLabel, aValue) {
    var menupopup = this.menupopup;
    if (!menupopup) {
      menupopup = this.ownerDocument.createXULElement("menupopup");
      this.appendChild(menupopup);
    }

    var menuitem = this.ownerDocument.createXULElement("menuitem");
    menuitem.setAttribute("label", aLabel);
    menuitem.setAttribute("value", aValue);

    return menupopup.appendChild(menuitem);
  }

  getIndexOfItem(aItem) {
    var menupopup = this.menupopup;
    if (menupopup) {
      var items = menupopup.children;
      var length = items.length;
      for (var index = 0; index < length; ++index) {
        if (items[index] == aItem)
          return index;
      }
    }
    return -1;
  }

  getItemAtIndex(aIndex) {
    var menupopup = this.menupopup;
    if (!menupopup || aIndex < 0 || aIndex >= menupopup.children.length)
      return null;

    return menupopup.children[aIndex];
  }
}

MozXULElement.implementCustomInterface(MozMenuBase, [Ci.nsIDOMXULContainerElement]);

// The <menucaption> element is used for rendering <html:optgroup> inside of <html:select>,
// See SelectParentHelper.jsm.
class MozMenuCaption extends MozMenuBase {
  static get inheritedAttributes() {
    return {
      ".menu-iconic-left": "selected,disabled,checked",
      ".menu-iconic-icon": "src=image,validate,src",
      ".menu-iconic-text": "value=label,crop,highlightable",
      ".menu-iconic-highlightable-text": "text=label,crop,highlightable",
    };
  }

  connectedCallback() {
    this.textContent = "";
    this.appendChild(MozXULElement.parseXULToFragment(`
      <hbox class="menu-iconic-left" align="center" pack="center" role="none">
        <image class="menu-iconic-icon" role="none"></image>
      </hbox>
      <label class="menu-iconic-text" flex="1" crop="right" role="none"></label>
      <label class="menu-iconic-highlightable-text" crop="right" role="none"></label>
    `));
    this.initializeAttributeInheritance();
  }
}

customElements.define("menucaption", MozMenuCaption);
}
