/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const DEBUG = false;
function debug(aStr) {
  if (DEBUG) {
    dump("-*- DateTimePickerHelper: " + aStr + "\n");
  }
}

this.EXPORTED_SYMBOLS = [
  "DateTimePickerHelper"
];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

/*
 * DateTimePickerHelper receives message from content side (input box) and
 * is reposible for opening, closing and updating the picker. Similary,
 * DateTimePickerHelper listens for picker's events and notifies the content
 * side (input box) about them.
 */
this.DateTimePickerHelper = {
  picker: null,
  weakBrowser: null,

  MESSAGES: [
    "FormDateTime:OpenPicker",
    "FormDateTime:ClosePicker",
    "FormDateTime:UpdatePicker"
  ],

  init() {
    for (let msg of this.MESSAGES) {
      Services.mm.addMessageListener(msg, this);
    }
  },

  uninit() {
    for (let msg of this.MESSAGES) {
      Services.mm.removeMessageListener(msg, this);
    }
  },

  // nsIMessageListener
  receiveMessage(aMessage) {
    debug("receiveMessage: " + aMessage.name);
    switch (aMessage.name) {
      case "FormDateTime:OpenPicker": {
        this.showPicker(aMessage.target, aMessage.data);
        break;
      }
      case "FormDateTime:ClosePicker": {
        if (!this.picker) {
          return;
        }
        this.picker.closePicker();
        break;
      }
      case "FormDateTime:UpdatePicker": {
        this.picker.setPopupValue(aMessage.data);
        break;
      }
      default:
        break;
    }
  },

  // nsIDOMEventListener
  handleEvent(aEvent) {
    debug("handleEvent: " + aEvent.type);
    switch (aEvent.type) {
      case "DateTimePickerValueChanged": {
        this.updateInputBoxValue(aEvent);
        break;
      }
      case "popuphidden": {
        let browser = this.weakBrowser ? this.weakBrowser.get() : null;
        if (browser) {
          browser.messageManager.sendAsyncMessage("FormDateTime:PickerClosed");
        }
        this.close();
        break;
      }
      default:
        break;
    }
  },

  // Called when picker value has changed, notify input box about it.
  updateInputBoxValue(aEvent) {
    let browser = this.weakBrowser ? this.weakBrowser.get() : null;
    if (browser) {
      browser.messageManager.sendAsyncMessage(
        "FormDateTime:PickerValueChanged", aEvent.detail);
    }
  },

  // Get picker from browser and show it anchored to the input box.
  showPicker: Task.async(function* (aBrowser, aData) {
    let rect = aData.rect;
    let type = aData.type;
    let detail = aData.detail;

    this._anchor = aBrowser.ownerGlobal.gBrowser.popupAnchor;
    this._anchor.left = rect.left;
    this._anchor.top = rect.top;
    this._anchor.width = rect.width;
    this._anchor.height = rect.height;
    this._anchor.hidden = false;

    debug("Opening picker with details: " + JSON.stringify(detail));

    let window = aBrowser.ownerGlobal;
    let tabbrowser = window.gBrowser;
    if (Services.focus.activeWindow != window ||
        tabbrowser.selectedBrowser != aBrowser) {
      // We were sent a message from a window or tab that went into the
      // background, so we'll ignore it for now.
      return;
    }

    this.weakBrowser = Cu.getWeakReference(aBrowser);
    this.picker = aBrowser.dateTimePicker;
    if (!this.picker) {
      debug("aBrowser.dateTimePicker not found, exiting now.");
      return;
    }
    // The datetimepopup binding is only attached when it is needed.
    // Check if openPicker method is present to determine if binding has
    // been attached. If not, attach the binding first before calling it.
    if (!this.picker.openPicker) {
      let bindingPromise = new Promise(resolve => {
        this.picker.addEventListener("DateTimePickerBindingReady",
                                     resolve, {once: true});
      });
      this.picker.setAttribute("active", true);
      yield bindingPromise;
    }
    // The arrow panel needs an anchor to work. The popupAnchor (this._anchor)
    // is a transparent div that the arrow can point to.
    this.picker.openPicker(type, this._anchor, detail);

    this.addPickerListeners();
  }),

  // Picker is closed, do some cleanup.
  close() {
    this.removePickerListeners();
    this.picker = null;
    this.weakBrowser = null;
    this._anchor.hidden = true;
  },

  // Listen to picker's event.
  addPickerListeners() {
    if (!this.picker) {
      return;
    }
    this.picker.addEventListener("popuphidden", this);
    this.picker.addEventListener("DateTimePickerValueChanged", this);
  },

  // Stop listening to picker's event.
  removePickerListeners() {
    if (!this.picker) {
      return;
    }
    this.picker.removeEventListener("popuphidden", this);
    this.picker.removeEventListener("DateTimePickerValueChanged", this);
  },
};
