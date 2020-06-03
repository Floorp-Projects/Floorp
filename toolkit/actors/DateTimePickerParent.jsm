/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(aStr) {
  if (DEBUG) {
    dump("-*- DateTimePickerParent: " + aStr + "\n");
  }
}

var EXPORTED_SYMBOLS = ["DateTimePickerParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "DateTimePickerPanel",
  "resource://gre/modules/DateTimePickerPanel.jsm"
);

/*
 * DateTimePickerParent receives message from content side (input box) and
 * is reposible for opening, closing and updating the picker. Similarly,
 * DateTimePickerParent listens for picker's events and notifies the content
 * side (input box) about them.
 */
class DateTimePickerParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    debug("receiveMessage: " + aMessage.name);
    switch (aMessage.name) {
      case "FormDateTime:OpenPicker": {
        let topBrowsingContext = this.manager.browsingContext.top;
        let browser = topBrowsingContext.embedderElement;
        this.showPicker(browser, aMessage.data);
        break;
      }
      case "FormDateTime:ClosePicker": {
        if (!this._picker) {
          return;
        }
        this._picker.closePicker();
        this.close();
        break;
      }
      case "FormDateTime:UpdatePicker": {
        if (!this._picker) {
          return;
        }
        this._picker.setPopupValue(aMessage.data);
        break;
      }
      default:
        break;
    }
  }

  handleEvent(aEvent) {
    debug("handleEvent: " + aEvent.type);
    switch (aEvent.type) {
      case "DateTimePickerValueChanged": {
        this.sendAsyncMessage("FormDateTime:PickerValueChanged", aEvent.detail);
        break;
      }
      case "popuphidden": {
        this.sendAsyncMessage("FormDateTime:PickerClosed", {});
        this._picker.closePicker();
        this.close();
        break;
      }
      default:
        break;
    }
  }

  // Get picker from browser and show it anchored to the input box.
  showPicker(aBrowser, aData) {
    let rect = aData.rect;
    let type = aData.type;
    let detail = aData.detail;

    this._anchor = aBrowser.popupAnchor;
    if (!this._anchor) {
      throw new Error(
        "No popup anchor for this browser, cannot show date picker"
      );
    }

    this._anchor.style.left = rect.left + "px";
    this._anchor.style.top = rect.top + "px";
    this._anchor.style.width = rect.width + "px";
    this._anchor.style.height = rect.height + "px";
    this._anchor.hidden = false;

    debug("Opening picker with details: " + JSON.stringify(detail));

    let window = aBrowser.ownerGlobal;
    let tabbrowser = window.gBrowser;
    if (
      Services.focus.activeWindow != window ||
      (tabbrowser && tabbrowser.selectedBrowser != aBrowser)
    ) {
      // We were sent a message from a window or tab that went into the
      // background, so we'll ignore it for now.
      return;
    }

    let panel;
    if (tabbrowser) {
      panel = tabbrowser._getAndMaybeCreateDateTimePickerPanel();
    } else {
      panel = aBrowser.dateTimePicker;
    }
    if (!panel) {
      debug("aBrowser.dateTimePicker not found, exiting now.");
      return;
    }
    this._picker = new DateTimePickerPanel(panel);
    // The arrow panel needs an anchor to work. The popupAnchor (this._anchor)
    // is a transparent div that the arrow can point to.
    this._picker.openPicker(type, this._anchor, detail);

    this.addPickerListeners();
  }

  // Picker is closed, do some cleanup.
  close() {
    this.removePickerListeners();
    this._picker = null;
    this._anchor.hidden = true;
  }

  // Listen to picker's event.
  addPickerListeners() {
    if (!this._picker) {
      return;
    }
    this._picker.element.addEventListener("popuphidden", this);
    this._picker.element.addEventListener("DateTimePickerValueChanged", this);
  }

  // Stop listening to picker's event.
  removePickerListeners() {
    if (!this._picker) {
      return;
    }
    this._picker.element.removeEventListener("popuphidden", this);
    this._picker.element.removeEventListener(
      "DateTimePickerValueChanged",
      this
    );
  }
}
