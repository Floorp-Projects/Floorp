/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEBUG = false;
function debug(aStr) {
  if (DEBUG) {
    dump("-*- DateTimePickerParent: " + aStr + "\n");
  }
}

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  DateTimePickerPanel: "resource://gre/modules/DateTimePickerPanel.sys.mjs",
});

/*
 * DateTimePickerParent receives message from content side (input box) and
 * is reposible for opening, closing and updating the picker. Similarly,
 * DateTimePickerParent listens for picker's events and notifies the content
 * side (input box) about them.
 */
export class DateTimePickerParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    debug("receiveMessage: " + aMessage.name);
    switch (aMessage.name) {
      case "FormDateTime:OpenPicker": {
        this.showPicker(aMessage.data);
        break;
      }
      case "FormDateTime:ClosePicker": {
        if (!this._picker) {
          return;
        }
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
      case "DateTimePickerValueCleared": {
        this.sendAsyncMessage("FormDateTime:PickerValueChanged", null);
        break;
      }
      case "DateTimePickerValueChanged": {
        this.sendAsyncMessage("FormDateTime:PickerValueChanged", aEvent.detail);
        break;
      }
      case "popuphidden": {
        this.sendAsyncMessage("FormDateTime:PickerClosed", {});
        this.close();
        break;
      }
      default:
        break;
    }
  }

  // Get picker from browser and show it anchored to the input box.
  showPicker(aData) {
    let rect = aData.rect;
    let type = aData.type;
    let detail = aData.detail;

    debug("Opening picker with details: " + JSON.stringify(detail));
    let topBC = this.browsingContext.top;
    let window = topBC.topChromeWindow;
    if (Services.focus.activeWindow != window) {
      debug("Not in the active window");
      return;
    }

    {
      let browser = topBC.embedderElement;
      if (
        browser &&
        browser.ownerGlobal.gBrowser &&
        browser.ownerGlobal.gBrowser.selectedBrowser != browser
      ) {
        debug("In background tab");
        return;
      }
    }

    let doc = window.document;
    let panel = doc.getElementById("DateTimePickerPanel");
    if (!panel) {
      panel = doc.createXULElement("panel");
      panel.id = "DateTimePickerPanel";
      panel.setAttribute("type", "arrow");
      panel.setAttribute("orient", "vertical");
      panel.setAttribute("ignorekeys", "true");
      panel.setAttribute("noautofocus", "true");
      // This ensures that clicks on the anchored input box are never consumed.
      panel.setAttribute("consumeoutsideclicks", "never");
      panel.setAttribute("level", "parent");
      panel.setAttribute("tabspecific", "true");
      let container =
        doc.getElementById("mainPopupSet") ||
        doc.querySelector("popupset") ||
        doc.documentElement.appendChild(doc.createXULElement("popupset"));
      container.appendChild(panel);
    }
    this._oldFocus = doc.activeElement;
    this._picker = new lazy.DateTimePickerPanel(panel);
    this._picker.openPicker(type, rect, detail);
    this.addPickerListeners();
  }

  // Close the picker and do some cleanup.
  close() {
    this._picker.closePicker();
    // Restore focus to where it was before the picker opened.
    this._oldFocus?.focus();
    this._oldFocus = null;
    this.removePickerListeners();
    this._picker = null;
  }

  // Listen to picker's event.
  addPickerListeners() {
    if (!this._picker) {
      return;
    }
    this._picker.element.addEventListener("popuphidden", this);
    this._picker.element.addEventListener("DateTimePickerValueChanged", this);
    this._picker.element.addEventListener("DateTimePickerValueCleared", this);
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
    this._picker.element.removeEventListener(
      "DateTimePickerValueCleared",
      this
    );
  }
}
