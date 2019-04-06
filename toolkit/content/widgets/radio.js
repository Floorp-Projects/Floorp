/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
(() => {
class MozRadiogroup extends MozElements.BaseControl {
  constructor() {
    super();

    this.addEventListener("mousedown", (event) => {
      if (this.disabled)
        event.preventDefault();
    });

    /**
     * keyboard navigation  Here's how keyboard navigation works in radio groups on Windows:
     * The group takes 'focus'
     * The user is then free to navigate around inside the group
     * using the arrow keys. Accessing previous or following radio buttons
     * is done solely through the arrow keys and not the tab button. Tab
     * takes you to the next widget in the tab order
     */
    this.addEventListener("keypress", (event) => {
      if (event.key != " " || event.originalTarget != this) {
        return;
      }
      this.selectedItem = this.focusedItem;
      this.selectedItem.doCommand();
      // Prevent page from scrolling on the space key.
      event.preventDefault();
    });

    this.addEventListener("keypress", (event) => {
      if (event.keyCode != KeyEvent.DOM_VK_UP || event.originalTarget != this) {
        return;
      }
      this.checkAdjacentElement(false);
      event.stopPropagation();
      event.preventDefault();
    });

    this.addEventListener("keypress", (event) => {
      if (event.keyCode != KeyEvent.DOM_VK_LEFT || event.originalTarget != this) {
        return;
      }
      // left arrow goes back when we are ltr, forward when we are rtl
      this.checkAdjacentElement(document.defaultView.getComputedStyle(
        this).direction == "rtl");
      event.stopPropagation();
      event.preventDefault();
    });

    this.addEventListener("keypress", (event) => {
      if (event.keyCode != KeyEvent.DOM_VK_DOWN || event.originalTarget != this) {
        return;
      }
      this.checkAdjacentElement(true);
      event.stopPropagation();
      event.preventDefault();
    });

    this.addEventListener("keypress", (event) => {
      if (event.keyCode != KeyEvent.DOM_VK_RIGHT || event.originalTarget != this) {
        return;
      }
      // right arrow goes forward when we are ltr, back when we are rtl
      this.checkAdjacentElement(document.defaultView.getComputedStyle(
        this).direction == "ltr");
      event.stopPropagation();
      event.preventDefault();
    });

    /**
     * set a focused attribute on the selected item when the group
     * receives focus so that we can style it as if it were focused even though
     * it is not (Windows platform behaviour is for the group to receive focus,
     * not the item
     */
    this.addEventListener("focus", (event) => {
      if (event.originalTarget != this) {
        return;
      }
      this.setAttribute("focused", "true");
      if (this.focusedItem)
        return;

      var val = this.selectedItem;
      if (!val || val.disabled || val.hidden || val.collapsed) {
        var children = this._getRadioChildren();
        for (var i = 0; i < children.length; ++i) {
          if (!children[i].hidden && !children[i].collapsed && !children[i].disabled) {
            val = children[i];
            break;
          }
        }
      }
      this.focusedItem = val;
    });

    this.addEventListener("blur", (event) => {
      if (event.originalTarget != this) {
        return;
      }
      this.removeAttribute("focused");
      this.focusedItem = null;
    });
  }

  connectedCallback() {
    if (this.delayConnectedCallback()) {
      return;
    }

    // When this is called via `connectedCallback` there are two main variations:
    //   1) The radiogroup and radio children are defined in markup.
    //   2) We are appending a DocumentFragment
    // In both cases, the <radiogroup> connectedCallback fires first. But in (2),
    // the children <radio>s won't be upgraded yet, so r.control will be undefined.
    // To avoid churn in this case where we would have to reinitialize the list as each
    // child radio gets upgraded as a result of init(), ignore the resulting calls
    // to radioAttached.
    this.ignoreRadioChildConstruction = true;
    this.init();
    this.ignoreRadioChildConstruction = false;
    if (!this.value) {
      this.selectedIndex = 0;
    }
  }

  init() {
    this._radioChildren = null;

    if (this.getAttribute("disabled") == "true")
      this.disabled = true;

    var children = this._getRadioChildren();
    var length = children.length;
    for (var i = 0; i < length; i++) {
      if (children[i].getAttribute("selected") == "true") {
        this.selectedIndex = i;
        return;
      }
    }

    var value = this.value;
    if (value)
      this.value = value;
  }

  /**
   * Called when a new <radio> gets added to an already connected radiogroup.
   * This can happen due to DOM getting appended after the <radiogroup> is created.
   * When this happens, reinitialize the UI if necessary to make sure the state is
   * consistent.
   *
   * @param {DOMNode} child
   *                  The <radio> element that got added
   */
  radioAttached(child) {
    if (this.ignoreRadioChildConstruction) {
      return;
    }
    if (!this._radioChildren || !this._radioChildren.includes(child)) {
      this.init();
    }
  }

  /**
   * Called when a new <radio> gets removed from a radio group.
   *
   * @param {DOMNode} child
   *                  The <radio> element that got removed
   */
  radioUnattached(child) {
    // Just invalidate the cache, next time it's fetched it'll get rebuilt.
    this._radioChildren = null;
  }

  set value(val) {
    this.setAttribute("value", val);
    var children = this._getRadioChildren();
    for (var i = 0; i < children.length; i++) {
      if (String(children[i].value) == String(val)) {
        this.selectedItem = children[i];
        break;
      }
    }
    return val;
  }

  get value() {
    return this.getAttribute("value");
  }

  set disabled(val) {
    if (val)
      this.setAttribute("disabled", "true");
    else
      this.removeAttribute("disabled");
    var children = this._getRadioChildren();
    for (var i = 0; i < children.length; ++i) {
      children[i].disabled = val;
    }
    return val;
  }

  get disabled() {
    if (this.getAttribute("disabled") == "true")
      return true;
    var children = this._getRadioChildren();
    for (var i = 0; i < children.length; ++i) {
      if (!children[i].hidden && !children[i].collapsed && !children[i].disabled)
        return false;
    }
    return true;
  }

  get itemCount() {
    return this._getRadioChildren().length;
  }

  set selectedIndex(val) {
    this.selectedItem = this._getRadioChildren()[val];
    return val;
  }

  get selectedIndex() {
    var children = this._getRadioChildren();
    for (var i = 0; i < children.length; ++i) {
      if (children[i].selected)
        return i;
    }
    return -1;
  }

  set selectedItem(val) {
    var focused = this.getAttribute("focused") == "true";
    var alreadySelected = false;

    if (val) {
      alreadySelected = val.getAttribute("selected") == "true";
      val.setAttribute("focused", focused);
      val.setAttribute("selected", "true");
      this.setAttribute("value", val.value);
    } else {
      this.removeAttribute("value");
    }

    // uncheck all other group nodes
    var children = this._getRadioChildren();
    var previousItem = null;
    for (var i = 0; i < children.length; ++i) {
      if (children[i] != val) {
        if (children[i].getAttribute("selected") == "true")
          previousItem = children[i];

        children[i].removeAttribute("selected");
        children[i].removeAttribute("focused");
      }
    }

    var event = document.createEvent("Events");
    event.initEvent("select", false, true);
    this.dispatchEvent(event);

    if (focused) {
      if (alreadySelected) {
        // Notify accessibility that this item got focus.
        event = document.createEvent("Events");
        event.initEvent("DOMMenuItemActive", true, true);
        val.dispatchEvent(event);
      } else {
        // Only report if actual change
        if (val) {
          // Accessibility will fire focus for this.
          event = document.createEvent("Events");
          event.initEvent("RadioStateChange", true, true);
          val.dispatchEvent(event);
        }

        if (previousItem) {
          event = document.createEvent("Events");
          event.initEvent("RadioStateChange", true, true);
          previousItem.dispatchEvent(event);
        }
      }
    }

    return val;
  }

  get selectedItem() {
    var children = this._getRadioChildren();
    for (var i = 0; i < children.length; ++i) {
      if (children[i].selected)
        return children[i];
    }
    return null;
  }

  set focusedItem(val) {
    if (val) {
      val.setAttribute("focused", "true");
      // Notify accessibility that this item got focus.
      let event = document.createEvent("Events");
      event.initEvent("DOMMenuItemActive", true, true);
      val.dispatchEvent(event);
    }

    // unfocus all other group nodes
    var children = this._getRadioChildren();
    for (var i = 0; i < children.length; ++i) {
      if (children[i] != val)
        children[i].removeAttribute("focused");
    }
    return val;
  }

  get focusedItem() {
    var children = this._getRadioChildren();
    for (var i = 0; i < children.length; ++i) {
      if (children[i].getAttribute("focused") == "true")
        return children[i];
    }
    return null;
  }

  checkAdjacentElement(aNextFlag) {
    var currentElement = this.focusedItem || this.selectedItem;
    var i;
    var children = this._getRadioChildren();
    for (i = 0; i < children.length; ++i) {
      if (children[i] == currentElement)
        break;
    }
    var index = i;

    if (aNextFlag) {
      do {
        if (++i == children.length)
          i = 0;
        if (i == index)
          break;
      }
      while (children[i].hidden || children[i].collapsed || children[i].disabled);
      // XXX check for display/visibility props too

      this.selectedItem = children[i];
      children[i].doCommand();
    } else {
      do {
        if (i == 0)
          i = children.length;
        if (--i == index)
          break;
      }
      while (children[i].hidden || children[i].collapsed || children[i].disabled);
      // XXX check for display/visibility props too

      this.selectedItem = children[i];
      children[i].doCommand();
    }
  }

  _getRadioChildren() {
    if (this._radioChildren)
      return this._radioChildren;

    let radioChildren = [];
    if (this.hasChildNodes()) {
      for (let radio of this.querySelectorAll("radio")) {
        customElements.upgrade(radio);
        if (radio.control == this) {
          radioChildren.push(radio);
        }
      }
    } else {
      const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      for (let radio of this.ownerDocument.getElementsByAttribute("group", this.id)) {
        if ((radio.namespaceURI == XUL_NS) &&
            (radio.localName == "radio")) {
          customElements.upgrade(radio);
          radioChildren.push(radio);
        }
      }
    }

    return this._radioChildren = radioChildren;
  }

  getIndexOfItem(item) {
    return this._getRadioChildren().indexOf(item);
  }

  getItemAtIndex(index) {
    var children = this._getRadioChildren();
    return (index >= 0 && index < children.length) ? children[index] : null;
  }

  appendItem(label, value) {
    var radio = document.createXULElement("radio");
    radio.setAttribute("label", label);
    radio.setAttribute("value", value);
    this.appendChild(radio);
    return radio;
  }
}

MozXULElement.implementCustomInterface(MozRadiogroup, [
  Ci.nsIDOMXULSelectControlElement,
  Ci.nsIDOMXULRadioGroupElement,
]);

customElements.define("radiogroup", MozRadiogroup);

let gRadioFrag = null;
function getRadioFragment() {
  if (!gRadioFrag) {
    gRadioFrag = MozXULElement.parseXULToFragment(`
    <image class="radio-check"></image>
    <hbox class="radio-label-box" align="center" flex="1">
      <image class="radio-icon"></image>
      <label class="radio-label" flex="1"></label>
    </hbox>
    `);
  }
  return document.importNode(gRadioFrag, true);
}

class MozRadio extends MozElements.BaseText {
  static get inheritedAttributes() {
    return {
      ".radio-check": "disabled,selected",
      ".radio-label": "text=label,accesskey,crop",
      ".radio-icon": "src",
    };
  }

  constructor() {
    super();
    this.addEventListener("click", (event) => {
      if (!this.disabled)
        this.control.selectedItem = this;
    });

    this.addEventListener("mousedown", (event) => {
      if (!this.disabled)
        this.control.focusedItem = this;
    });
  }

  connectedCallback() {
    if (this.delayConnectedCallback()) {
      return;
    }

    if (!this.connectedOnce) {
      this.connectedOnce = true;
      // If the caller didn't provide custom content then append the default:
      if (!this.firstElementChild) {
        this.appendChild(getRadioFragment());
        this.initializeAttributeInheritance();
      }
    }

    var control = this._control = this.control;
    if (control) {
      control.radioAttached(this);
    }
  }

  disconnectedCallback() {
    if (this.control) {
      this.control.radioUnattached(this);
    }
    this._control = null;
  }

  set value(val) {
    this.setAttribute("value", val);
  }

  get value() {
    return this.getAttribute("value");
  }

  get selected() {
    return this.hasAttribute("selected");
  }

  get radioGroup() {
    return this.control;
  }

  get control() {
    if (this._control) {
      return this._control;
    }

    var radiogroup = this.closest("radiogroup");
    if (radiogroup) {
      return radiogroup;
    }

    var group = this.getAttribute("group");
    if (!group) {
      return null;
    }

    var parent = this.ownerDocument.getElementById(group);
    if (!parent || parent.localName != "radiogroup") {
      parent = null;
    }
    return parent;
  }
}

MozXULElement.implementCustomInterface(MozRadio, [Ci.nsIDOMXULSelectControlItemElement]);
customElements.define("radio", MozRadio);
})();
