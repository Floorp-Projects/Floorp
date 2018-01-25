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
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");

Cu.importGlobalProperties(["InspectorUtils"]);

const kStateActive = 0x00000001; // NS_EVENT_STATE_ACTIVE
const kStateHover = 0x00000004; // NS_EVENT_STATE_HOVER

const SUPPORTED_PROPERTIES = [
  "color",
  "background-color",
  "text-shadow",
];

// A process global state for whether or not content thinks
// that a <select> dropdown is open or not. This is managed
// entirely within this module, and is read-only accessible
// via SelectContentHelper.open.
var gOpen = false;

this.EXPORTED_SYMBOLS = [
  "SelectContentHelper"
];

this.SelectContentHelper = function(aElement, aOptions, aGlobal) {
  this.element = aElement;
  this.initialSelection = aElement[aElement.selectedIndex] || null;
  this.global = aGlobal;
  this.closedWithClickOn = false;
  this.isOpenedViaTouch = aOptions.isOpenedViaTouch;
  this._selectBackgroundColor = null;
  this._selectColor = null;
  this._uaBackgroundColor = null;
  this._uaColor = null;
  this._uaSelectBackgroundColor = null;
  this._uaSelectColor = null;
  this._closeAfterBlur = true;
  this._pseudoStylesSetup = false;
  this._lockedDescendants = null;
  this.init();
  this.showDropDown();
  this._updateTimer = new DeferredTask(this._update.bind(this), 0);
};

Object.defineProperty(SelectContentHelper, "open", {
  get() {
    return gOpen;
  },
});

this.SelectContentHelper.prototype = {
  init() {
    this.global.addMessageListener("Forms:SelectDropDownItem", this);
    this.global.addMessageListener("Forms:DismissedDropDown", this);
    this.global.addMessageListener("Forms:MouseOver", this);
    this.global.addMessageListener("Forms:MouseOut", this);
    this.global.addMessageListener("Forms:MouseUp", this);
    this.global.addMessageListener("Forms:SearchFocused", this);
    this.global.addMessageListener("Forms:BlurDropDown-Pong", this);
    this.global.addEventListener("pagehide", this, { mozSystemGroup: true });
    this.global.addEventListener("mozhidedropdown", this, { mozSystemGroup: true });
    this.element.addEventListener("blur", this, { mozSystemGroup: true });
    this.element.addEventListener("transitionend", this, { mozSystemGroup: true });
    let MutationObserver = this.element.ownerGlobal.MutationObserver;
    this.mut = new MutationObserver(mutations => {
      // Something changed the <select> while it was open, so
      // we'll poke a DeferredTask to update the parent sometime
      // in the very near future.
      this._updateTimer.arm();
    });
    this.mut.observe(this.element, {childList: true, subtree: true, attributes: true});
  },

  uninit() {
    this.element.openInParentProcess = false;
    this.global.removeMessageListener("Forms:SelectDropDownItem", this);
    this.global.removeMessageListener("Forms:DismissedDropDown", this);
    this.global.removeMessageListener("Forms:MouseOver", this);
    this.global.removeMessageListener("Forms:MouseOut", this);
    this.global.removeMessageListener("Forms:MouseUp", this);
    this.global.removeMessageListener("Forms:SearchFocused", this);
    this.global.removeMessageListener("Forms:BlurDropDown-Pong", this);
    this.global.removeEventListener("pagehide", this, { mozSystemGroup: true });
    this.global.removeEventListener("mozhidedropdown", this, { mozSystemGroup: true });
    this.element.removeEventListener("blur", this, { mozSystemGroup: true });
    this.element.removeEventListener("transitionend", this, { mozSystemGroup: true });
    this.element = null;
    this.global = null;
    this.mut.disconnect();
    this._updateTimer.disarm();
    this._updateTimer = null;
    gOpen = false;
  },

  showDropDown() {
    this.element.openInParentProcess = true;
    this._setupPseudoClassStyles();
    let rect = this._getBoundingContentRect();
    let computedStyles = getComputedStyles(this.element);
    this._selectBackgroundColor = computedStyles.backgroundColor;
    this._selectColor = computedStyles.color;
    this._selectTextShadow = computedStyles.textShadow;
    this.global.sendAsyncMessage("Forms:ShowDropDown", {
      direction: computedStyles.direction,
      isOpenedViaTouch: this.isOpenedViaTouch,
      options: this._buildOptionList(),
      rect,
      selectedIndex: this.element.selectedIndex,
      selectBackgroundColor: this._selectBackgroundColor,
      selectColor: this._selectColor,
      selectTextShadow: this._selectTextShadow,
      uaBackgroundColor: this.uaBackgroundColor,
      uaColor: this.uaColor,
      uaSelectBackgroundColor: this.uaSelectBackgroundColor,
      uaSelectColor: this.uaSelectColor
    });
    this._clearPseudoClassStyles();
    gOpen = true;
  },

  _setupPseudoClassStyles() {
    if (this._pseudoStylesSetup) {
      throw new Error("pseudo styles must not be set up yet");
    }
    // Do all of the things that change style at once, before we read
    // any styles.
    this._pseudoStylesSetup = true;
    InspectorUtils.addPseudoClassLock(this.element, ":focus");
    let lockedDescendants = this._lockedDescendants = this.element.querySelectorAll(":checked");
    for (let child of lockedDescendants) {
      // Selected options have the :checked pseudo-class, which
      // we want to disable before calculating the computed
      // styles since the user agent styles alter the styling
      // based on :checked.
      InspectorUtils.addPseudoClassLock(child, ":checked", false);
    }
  },

  _clearPseudoClassStyles() {
    if (!this._pseudoStylesSetup) {
      throw new Error("pseudo styles must be set up already");
    }
    // Undo all of the things that change style at once, after we're
    // done reading styles.
    InspectorUtils.clearPseudoClassLocks(this.element);
    let lockedDescendants = this._lockedDescendants;
    for (let child of lockedDescendants) {
      InspectorUtils.clearPseudoClassLocks(child);
    }
    this._lockedDescendants = null;
    this._pseudoStylesSetup = false;
  },

  _getBoundingContentRect() {
    return BrowserUtils.getElementBoundingScreenRect(this.element);
  },

  _buildOptionList() {
    if (!this._pseudoStylesSetup) {
      throw new Error("pseudo styles must be set up");
    }
    return buildOptionListForChildren(this.element);
  },

  _update() {
    // The <select> was updated while the dropdown was open.
    // Let's send up a new list of options.
    // Technically we might not need to set this pseudo-class
    // during _update() since the element should organically
    // have :focus, though it is here for belt-and-suspenders.
    this._setupPseudoClassStyles();
    let computedStyles = getComputedStyles(this.element);
    this._selectBackgroundColor = computedStyles.backgroundColor;
    this._selectColor = computedStyles.color;
    this._selectTextShadow = computedStyles.textShadow;
    this.global.sendAsyncMessage("Forms:UpdateDropDown", {
      options: this._buildOptionList(),
      selectedIndex: this.element.selectedIndex,
      selectBackgroundColor: this._selectBackgroundColor,
      selectColor: this._selectColor,
      selectTextShadow: this._selectTextShadow,
      uaBackgroundColor: this.uaBackgroundColor,
      uaColor: this.uaColor,
      uaSelectBackgroundColor: this.uaSelectBackgroundColor,
      uaSelectColor: this.uaSelectColor
    });
    this._clearPseudoClassStyles();
  },

  // Determine user agent background-color and color.
  // This is used to skip applying the custom color if it matches
  // the user agent values.
  _calculateUAColors() {
    let dummyOption = this.element.ownerDocument.createElementNS("http://www.w3.org/1999/xhtml", "option");
    dummyOption.style.setProperty("color", "-moz-comboboxtext", "important");
    dummyOption.style.setProperty("background-color", "-moz-combobox", "important");
    let optionCS = this.element.ownerGlobal.getComputedStyle(dummyOption);
    this._uaBackgroundColor = optionCS.backgroundColor;
    this._uaColor = optionCS.color;
    let dummySelect = this.element.ownerDocument.createElementNS("http://www.w3.org/1999/xhtml", "select");
    dummySelect.style.setProperty("color", "-moz-fieldtext", "important");
    dummySelect.style.setProperty("background-color", "-moz-field", "important");
    let selectCS = this.element.ownerGlobal.getComputedStyle(dummySelect);
    this._uaSelectBackgroundColor = selectCS.backgroundColor;
    this._uaSelectColor = selectCS.color;
  },

  get uaBackgroundColor() {
    if (!this._uaBackgroundColor) {
      this._calculateUAColors();
    }
    return this._uaBackgroundColor;
  },

  get uaColor() {
    if (!this._uaColor) {
      this._calculateUAColors();
    }
    return this._uaColor;
  },

  get uaSelectBackgroundColor() {
    if (!this._selectBackgroundColor) {
      this._calculateUAColors();
    }
    return this._uaSelectBackgroundColor;
  },

  get uaSelectColor() {
    if (!this._selectBackgroundColor) {
      this._calculateUAColors();
    }
    return this._uaSelectColor;
  },

  dispatchMouseEvent(win, target, eventName) {
    let mouseEvent = new win.MouseEvent(eventName, {
      view: win,
      bubbles: true,
      cancelable: true,
    });
    target.dispatchEvent(mouseEvent);
  },

  receiveMessage(message) {
    switch (message.name) {
      case "Forms:SelectDropDownItem":
        this.element.selectedIndex = message.data.value;
        this.closedWithClickOn = !message.data.closedWithEnter;
        break;

      case "Forms:DismissedDropDown": {
          let win = this.element.ownerGlobal;
          let selectedOption = this.element.item(this.element.selectedIndex);

          // For ordering of events, we're using non-e10s as our guide here,
          // since the spec isn't exactly clear. In non-e10s:
          // - If the user clicks on an element in the dropdown, we fire
          //   mousedown, mouseup, input, change, and click events.
          // - If the user uses the keyboard to select an element in the
          //   dropdown, we only fire input and change events.
          // - If the user pressed ESC key or clicks outside the dropdown,
          //   we fire nothing as the selected option is unchanged.
          if (this.closedWithClickOn) {
            this.dispatchMouseEvent(win, selectedOption, "mousedown");
            this.dispatchMouseEvent(win, selectedOption, "mouseup");
          }

          // Clear active document no matter user selects via keyboard or mouse
          InspectorUtils.removeContentState(this.element, kStateActive,
                                            /* aClearActiveDocument */ true);

          // Fire input and change events when selected option changes
          if (this.initialSelection !== selectedOption) {
            let inputEvent = new win.UIEvent("input", {
              bubbles: true,
            });
            this.element.dispatchEvent(inputEvent);

            let changeEvent = new win.Event("change", {
              bubbles: true,
            });
            this.element.dispatchEvent(changeEvent);
          }

          // Fire click event
          if (this.closedWithClickOn) {
            this.dispatchMouseEvent(win, selectedOption, "click");
          }

          this.uninit();
          break;
        }

      case "Forms:MouseOver":
        InspectorUtils.setContentState(this.element, kStateHover);
        break;

      case "Forms:MouseOut":
        InspectorUtils.removeContentState(this.element, kStateHover);
        break;

      case "Forms:MouseUp":
        let win = this.element.ownerGlobal;
        if (message.data.onAnchor) {
          this.dispatchMouseEvent(win, this.element, "mouseup");
        }
        InspectorUtils.removeContentState(this.element, kStateActive);
        if (message.data.onAnchor) {
          this.dispatchMouseEvent(win, this.element, "click");
        }
        break;

      case "Forms:SearchFocused":
        this._closeAfterBlur = false;
        break;

      case "Forms:BlurDropDown-Pong":
        if (!this._closeAfterBlur || !gOpen) {
          return;
        }
        this.global.sendAsyncMessage("Forms:HideDropDown", {});
        this.uninit();
        break;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "pagehide":
        if (this.element.ownerDocument === event.target) {
          this.global.sendAsyncMessage("Forms:HideDropDown", {});
          this.uninit();
        }
        break;
      case "blur": {
        if (this.element !== event.target) {
          break;
        }
        this._closeAfterBlur = true;
        // Send a ping-pong message to make sure that we wait for
        // enough cycles to pass from the potential focusing of the
        // search box to disable closing-after-blur.
        this.global.sendAsyncMessage("Forms:BlurDropDown-Ping", {});
        break;
      }
      case "mozhidedropdown":
        if (this.element === event.target) {
          this.global.sendAsyncMessage("Forms:HideDropDown", {});
          this.uninit();
        }
        break;
      case "transitionend":
        if (SUPPORTED_PROPERTIES.indexOf(event.propertyName) != -1) {
          this._updateTimer.arm();
        }
        break;
    }
  }

};

function getComputedStyles(element) {
  return element.ownerGlobal.getComputedStyle(element);
}

function buildOptionListForChildren(node) {
  let result = [];

  for (let child of node.children) {
    let tagName = child.tagName.toUpperCase();

    if (tagName == "OPTION" || tagName == "OPTGROUP") {
      if (child.hidden) {
        continue;
      }

      let textContent =
        tagName == "OPTGROUP" ? child.getAttribute("label")
                              : child.text;
      if (textContent == null) {
        textContent = "";
      }

      let cs = getComputedStyles(child);

      // Note: If you add any more CSS properties support here,
      // please add the property name to the SUPPORTED_PROPERTIES
      // list so that the menu can be correctly updated when CSS
      // transitions are used.
      let info = {
        index: child.index,
        tagName,
        textContent,
        disabled: child.disabled,
        display: cs.display,
        // We need to do this for every option element as each one can have
        // an individual style set for direction
        textDirection: cs.direction,
        tooltip: child.title,
        backgroundColor: cs.backgroundColor,
        color: cs.color,
        children: tagName == "OPTGROUP" ? buildOptionListForChildren(child) : []
      };

      if (cs.textShadow != "none") {
        info.textShadow = cs.textShadow;
      }

      result.push(info);
    }
  }
  return result;
}
