/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "DOMUtils",
                                   "@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");

const kStateHover = 0x00000004; // NS_EVENT_STATE_HOVER

// A process global state for whether or not content thinks
// that a <select> dropdown is open or not. This is managed
// entirely within this module, and is read-only accessible
// via SelectContentHelper.open.
var gOpen = false;

this.EXPORTED_SYMBOLS = [
  "SelectContentHelper"
];

this.SelectContentHelper = function (aElement, aGlobal) {
  this.element = aElement;
  this.initialSelection = aElement[aElement.selectedIndex] || null;
  this.global = aGlobal;
  this.init();
  this.showDropDown();
  this._updateTimer = new DeferredTask(this._update.bind(this), 0);
}

Object.defineProperty(SelectContentHelper, "open", {
  get: function() {
    return gOpen;
  },
});

this.SelectContentHelper.prototype = {
  init: function() {
    this.global.addMessageListener("Forms:SelectDropDownItem", this);
    this.global.addMessageListener("Forms:DismissedDropDown", this);
    this.global.addMessageListener("Forms:MouseOver", this);
    this.global.addMessageListener("Forms:MouseOut", this);
    this.global.addEventListener("pagehide", this);
    this.global.addEventListener("mozhidedropdown", this);
    let MutationObserver = this.element.ownerDocument.defaultView.MutationObserver;
    this.mut = new MutationObserver(mutations => {
      // Something changed the <select> while it was open, so
      // we'll poke a DeferredTask to update the parent sometime
      // in the very near future.
      this._updateTimer.arm();
    });
    this.mut.observe(this.element, {childList: true, subtree: true});
  },

  uninit: function() {
    this.global.removeMessageListener("Forms:SelectDropDownItem", this);
    this.global.removeMessageListener("Forms:DismissedDropDown", this);
    this.global.removeMessageListener("Forms:MouseOver", this);
    this.global.removeMessageListener("Forms:MouseOut", this);
    this.global.removeEventListener("pagehide", this);
    this.global.removeEventListener("mozhidedropdown", this);
    this.element = null;
    this.global = null;
    this.mut.disconnect();
    this._updateTimer.disarm();
    this._updateTimer = null;
    gOpen = false;
  },

  showDropDown: function() {
    let rect = this._getBoundingContentRect();
    this.global.sendAsyncMessage("Forms:ShowDropDown", {
      rect: rect,
      options: this._buildOptionList(),
      selectedIndex: this.element.selectedIndex,
      direction: getComputedDirection(this.element)
    });
    gOpen = true;
  },

  _getBoundingContentRect: function() {
    return BrowserUtils.getElementBoundingScreenRect(this.element);
  },

  _buildOptionList: function() {
    return buildOptionListForChildren(this.element);
  },

  _update() {
    // The <select> was updated while the dropdown was open.
    // Let's send up a new list of options.
    this.global.sendAsyncMessage("Forms:UpdateDropDown", {
      options: this._buildOptionList(),
      selectedIndex: this.element.selectedIndex,
    });
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Forms:SelectDropDownItem":
        this.element.selectedIndex = message.data.value;
        break;

      case "Forms:DismissedDropDown":
        if (this.initialSelection != this.element.item(this.element.selectedIndex)) {
          let win = this.element.ownerDocument.defaultView;
          let inputEvent = new win.UIEvent("input", {
            bubbles: true,
          });
          this.element.dispatchEvent(inputEvent);

          let changeEvent = new win.Event("change", {
            bubbles: true,
          });
          this.element.dispatchEvent(changeEvent);

          let dwu = win.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
          let rect = this.element.getBoundingClientRect();
          dwu.sendMouseEvent("mousedown", rect.left, rect.top, 0, 1, 0, true);
          dwu.sendMouseEvent("mouseup", rect.left, rect.top, 0, 1, 0, true);
        }

        this.uninit();
        break;

      case "Forms:MouseOver":
        DOMUtils.setContentState(this.element, kStateHover);
        break;

      case "Forms:MouseOut":
        DOMUtils.removeContentState(this.element, kStateHover);
        break;

    }
  },

  handleEvent: function(event) {
    switch (event.type) {
      case "pagehide":
        if (this.element.ownerDocument === event.target) {
          this.global.sendAsyncMessage("Forms:HideDropDown", {});
          this.uninit();
        }
        break;
      case "mozhidedropdown":
        if (this.element === event.target) {
          this.global.sendAsyncMessage("Forms:HideDropDown", {});
          this.uninit();
        }
        break;
    }
  }

}

function getComputedDirection(element) {
  return element.ownerDocument.defaultView.getComputedStyle(element).getPropertyValue("direction");
}

function buildOptionListForChildren(node) {
  let result = [];

  for (let child of node.children) {
    let tagName = child.tagName.toUpperCase();

    if (tagName == 'OPTION' || tagName == 'OPTGROUP') {
      if (child.hidden) {
        continue;
      }

      let textContent =
        tagName == 'OPTGROUP' ? child.getAttribute("label")
                              : child.text;
      if (textContent == null) {
        textContent = "";
      }

      let info = {
        index: child.index,
        tagName: tagName,
        textContent: textContent,
        disabled: child.disabled,
        display: child.style.display,
        // We need to do this for every option element as each one can have
        // an individual style set for direction
        textDirection: getComputedDirection(child),
        tooltip: child.title,
        // XXX this uses a highlight color when this is the selected element.
        // We need to suppress such highlighting in the content process to get
        // the option's correct unhighlighted color here.
        // We also need to detect default color vs. custom so that a standard
        // color does not override color: menutext in the parent.
        // backgroundColor: computedStyle.backgroundColor,
        // color: computedStyle.color,
        children: tagName == 'OPTGROUP' ? buildOptionListForChildren(child) : []
      };
      result.push(info);
    }
  }
  return result;
}
