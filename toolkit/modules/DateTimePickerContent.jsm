/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");

var EXPORTED_SYMBOLS = ["DateTimePickerContent"];

/**
 * DateTimePickerContent is the communication channel between the input box
 * (content) for date/time input types and its picker (chrome).
 */
class DateTimePickerContent {
  /**
   * On init, just listen for the event to open the picker, once the picker is
   * opened, we'll listen for update and close events.
   */
  constructor(global) {
    this._inputElement = null;
    this._global = global;
  }

  /**
   * Cleanup function called when picker is closed.
   */
  close() {
    this.removeListeners();
    this._inputElement.setDateTimePickerState(false);
    this._inputElement = null;
  }

  /**
   * Called after picker is opened to start listening for input box update
   * events.
   */
  addListeners() {
    this._global.addEventListener("MozUpdateDateTimePicker", this);
    this._global.addEventListener("MozCloseDateTimePicker", this);
    this._global.addEventListener("pagehide", this);

    this._global.addMessageListener("FormDateTime:PickerValueChanged", this);
    this._global.addMessageListener("FormDateTime:PickerClosed", this);
  }

  /**
   * Stop listeneing for events when picker is closed.
   */
  removeListeners() {
    this._global.removeEventListener("MozUpdateDateTimePicker", this);
    this._global.removeEventListener("MozCloseDateTimePicker", this);
    this._global.removeEventListener("pagehide", this);

    this._global.removeMessageListener("FormDateTime:PickerValueChanged", this);
    this._global.removeMessageListener("FormDateTime:PickerClosed", this);
  }

  /**
   * Helper function that returns the CSS direction property of the element.
   */
  getComputedDirection(aElement) {
    return aElement.ownerGlobal.getComputedStyle(aElement)
      .getPropertyValue("direction");
  }

  /**
   * Helper function that returns the rect of the element, which is the position
   * relative to the left/top of the content area.
   */
  getBoundingContentRect(aElement) {
    return BrowserUtils.getElementBoundingRect(aElement);
  }

  getTimePickerPref() {
    return Services.prefs.getBoolPref("dom.forms.datetime.timepicker");
  }

  /**
   * nsIMessageListener.
   */
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "FormDateTime:PickerClosed": {
        this.close();
        break;
      }
      case "FormDateTime:PickerValueChanged": {
        this._inputElement.updateDateTimeInputBox(aMessage.data);
        break;
      }
      default:
        break;
    }
  }

  /**
   * nsIDOMEventListener, for chrome events sent by the input element and other
   * DOM events.
   */
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozOpenDateTimePicker": {
        // Time picker is disabled when preffed off
        if (!(aEvent.originalTarget instanceof aEvent.originalTarget.ownerGlobal.HTMLInputElement) ||
            (aEvent.originalTarget.type == "time" && !this.getTimePickerPref())) {
          return;
        }

        if (this._inputElement) {
          // This happens when we're trying to open a picker when another picker
          // is still open. We ignore this request to let the first picker
          // close gracefully.
          return;
        }

        this._inputElement = aEvent.originalTarget;
        this._inputElement.setDateTimePickerState(true);
        this.addListeners();

        let value = this._inputElement.getDateTimeInputBoxValue();
        this._global.sendAsyncMessage("FormDateTime:OpenPicker", {
          rect: this.getBoundingContentRect(this._inputElement),
          dir: this.getComputedDirection(this._inputElement),
          type: this._inputElement.type,
          detail: {
            // Pass partial value if it's available, otherwise pass input
            // element's value.
            value: Object.keys(value).length > 0 ? value
                                                 : this._inputElement.value,
            min: this._inputElement.getMinimum(),
            max: this._inputElement.getMaximum(),
            step: this._inputElement.getStep(),
            stepBase: this._inputElement.getStepBase(),
          },
        });
        break;
      }
      case "MozUpdateDateTimePicker": {
        let value = this._inputElement.getDateTimeInputBoxValue();
        value.type = this._inputElement.type;
        this._global.sendAsyncMessage("FormDateTime:UpdatePicker", { value });
        break;
      }
      case "MozCloseDateTimePicker": {
        this._global.sendAsyncMessage("FormDateTime:ClosePicker");
        this.close();
        break;
      }
      case "pagehide": {
        if (this._inputElement &&
            this._inputElement.ownerDocument == aEvent.target) {
          this._global.sendAsyncMessage("FormDateTime:ClosePicker");
          this.close();
        }
        break;
      }
      default:
        break;
    }
  }
}
