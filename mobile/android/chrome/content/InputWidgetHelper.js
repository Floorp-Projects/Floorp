/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var InputWidgetHelper = {
  _uiBusy: false,

  strings: function() {
    if (!this._strings) {
      this._strings = Services.strings.createBundle(
          "chrome://browser/locale/browser.properties");
    }
    return this._strings;
  },

  handleEvent: function(aEvent) {
    this.handleClick(aEvent.target);
  },

  handleClick: function(aTarget) {
    // if we're busy looking at a InputWidget we want to eat any clicks that
    // come to us, but not to process them
    if (this._uiBusy || !this.hasInputWidget(aTarget) || this._isDisabledElement(aTarget))
      return;

    this._uiBusy = true;
    this.show(aTarget);
    this._uiBusy = false;
  },

  show: function(aElement) {
    let type = aElement.getAttribute('type');
    let p = new Prompt({
      window: aElement.ownerGlobal,
      title: this.strings().GetStringFromName("inputWidgetHelper." + aElement.getAttribute('type')),
      buttons: [
        this.strings().GetStringFromName("inputWidgetHelper.set"),
        this.strings().GetStringFromName("inputWidgetHelper.clear"),
        this.strings().GetStringFromName("inputWidgetHelper.cancel")
      ],
    }).addDatePicker({
      value: aElement.value,
      type: type,
      min: aElement.min,
      max: aElement.max,
    }).show(data => {
      let changed = false;
      if (data.button == -1) {
        // This type is not supported with this android version.
        return;
      }
      if (data.button == 1) {
        // The user cleared the value.
        if (aElement.value != "") {
          aElement.value = "";
          changed = true;
        }
      } else if (data.button == 0) {
        // Commit the new value.
        if (aElement.value != data[type]) {
          aElement.value = data[type + "0"];
          changed = true;
        }
      }
      // Else the user canceled the input.

      if (changed)
        this.fireOnChange(aElement);
    });
  },

  hasInputWidget: function(aElement) {
    let win = aElement.ownerGlobal;
    if (!(aElement instanceof win.HTMLInputElement))
      return false;

    let type = aElement.getAttribute('type');
    if (type == "date" || type == "datetime" || type == "datetime-local" ||
        type == "week" || type == "month" || type == "time") {
      return true;
    }

    return false;
  },

  fireOnChange: function(aElement) {
    let win = aElement.ownerGlobal;
    let evt = aElement.ownerDocument.createEvent("Events");
    evt.initEvent("change", true, true, aElement.defaultView, 0,
                  false, false,
                  false, false, null);
    win.setTimeout(function() {
      aElement.dispatchEvent(evt);
    }, 0);
  },

  _isDisabledElement : function(aElement) {
    let currentElement = aElement;
    while (currentElement) {
      if (currentElement.disabled)
	return true;

      currentElement = currentElement.parentElement;
    }
    return false;
  }
};
