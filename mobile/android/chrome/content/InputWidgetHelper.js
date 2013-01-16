/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var InputWidgetHelper = {
  _uiBusy: false,

  handleEvent: function(aEvent) {
    this.handleClick(aEvent.target);
  },

  handleClick: function(aTarget) {
    // if we're busy looking at a InputWidget we want to eat any clicks that
    // come to us, but not to process them
    if (this._uiBusy || !this._isValidInput(aTarget))
      return;

    this._uiBusy = true;
    this.show(aTarget);
    this._uiBusy = false;
  },

  show: function(aElement) {
    let type = aElement.getAttribute('type');
    let msg = {
      type: "Prompt:Show",
      title: Strings.browser.GetStringFromName("inputWidgetHelper." + aElement.getAttribute('type')),
      buttons: [
        Strings.browser.GetStringFromName("inputWidgetHelper.set"),
        Strings.browser.GetStringFromName("inputWidgetHelper.clear"),
        Strings.browser.GetStringFromName("inputWidgetHelper.cancel")
      ],
      inputs: [
        { type: type, value: aElement.value }
      ]
    };

    let data = JSON.parse(sendMessageToJava({ gecko: msg }));

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
        aElement.value = data[type];
        changed = true;
      }
    }
    // Else the user canceled the input.

    if (changed)
      this.fireOnChange(aElement);

  },

  _isValidInput: function(aElement) {
    if (!aElement instanceof HTMLInputElement)
      return false;

    let type = aElement.getAttribute('type');
    if (type == "date" || type == "datetime" || type == "datetime-local" ||
        type == "week" || type == "month" || type == "time") {
      return true;
    }

    return false;
  },

  fireOnChange: function(aElement) {
    let evt = aElement.ownerDocument.createEvent("Events");
    evt.initEvent("change", true, true, aElement.defaultView, 0,
                  false, false,
                  false, false, null);
    setTimeout(function() {
      aElement.dispatchEvent(evt);
    }, 0);
  }
};
