/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

XPCOMUtils.defineLazyModuleGetter(this, "Prompt",
                                  "resource://gre/modules/Prompt.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
                                  "resource://gre/modules/UITelemetry.jsm");

const kPRESENTATIONDEVICEPROMPT_CONTRACTID = "@mozilla.org/presentation-device/prompt;1";
const kPRESENTATIONDEVICEPROMPT_CID        = Components.ID("{388bd149-c919-4a43-b646-d7ec57877689}");

function debug(aMsg) {
  // dump("-*- PresentationDevicePrompt: " + aMsg + "\n");
}

// nsIPresentationDevicePrompt
function PresentationDevicePrompt() {
  debug("PresentationDevicePrompt init");
}

PresentationDevicePrompt.prototype = {
  classID: kPRESENTATIONDEVICEPROMPT_CID,
  contractID: kPRESENTATIONDEVICEPROMPT_CONTRACTID,
  classDescription: "Fennec Presentation Device Prompt",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevicePrompt]),

  _devices: [],   // Store all available presentation devices
  _request: null, // Store the request from presentation api

  _getString: function(aName) {
    debug("_getString");

    if (!this.bundle) {
      this.bundle = Services.strings.createBundle("chrome://browser/locale/devicePrompt.properties");
    }
    return this.bundle.GetStringFromName(aName);
  },

  _loadDevices: function(requestURLs) {
    debug("_loadDevices");

    let deviceManager = Cc["@mozilla.org/presentation-device/manager;1"]
                          .getService(Ci.nsIPresentationDeviceManager);
    let devices = deviceManager.getAvailableDevices(requestURLs).QueryInterface(Ci.nsIArray);

    // Re-load the available devices
    this._devices = [];
    for (let i = 0; i < devices.length; i++) {
      let device = devices.queryElementAt(i, Ci.nsIPresentationDevice);
      this._devices.push(device);
    }
  },

  _getPromptMenu: function(aDevices) {
    debug("_getPromptMenu");

    return aDevices.map(function(device) {
      return { label: device.name };
    });
  },

  _getPrompt: function(aTitle, aMenu) {
    debug("_getPrompt");

    let p = new Prompt({
      title: aTitle,
    });

    p.setSingleChoiceItems(aMenu);

    return p;
  },

  _showPrompt: function(aPrompt, aCallback) {
    debug("_showPrompt");

    aPrompt.show(function(data) {
      let buttonIndex = data.button;
      aCallback(buttonIndex);
    });
  },

  _selectDevice: function(aIndex) {
    debug("_selectDevice");

    if (!this._request) {
      return;
    }

    if (aIndex < 0) {                    // Cancel request if no selected device,
      this._request.cancel(Cr.NS_ERROR_DOM_NOT_ALLOWED_ERR);
      return;
    } else if (!this._devices.length) {  // or there is no available devices
      this._request.cancel(Cr.NS_ERROR_DOM_NOT_FOUND_ERR);
      return;
    }

    this._request.select(this._devices[aIndex]);
  },

  // This will be fired when window.PresentationRequest(URL).start() is called
  promptDeviceSelection: function(aRequest) {
    debug("promptDeviceSelection");

    // Load available presentation devices into this._devices
    this._loadDevices(aRequest.requestURLs);

    if (!this._devices.length) { // Cancel request if no available device
      aRequest.cancel(Cr.NS_ERROR_DOM_NOT_FOUND_ERR);
      return;
    }

    this._request = aRequest;

    let prompt = this._getPrompt(this._getString("deviceMenu.title"),
                                 this._getPromptMenu(this._devices));

    this._showPrompt(prompt, this._selectDevice.bind(this));

    UITelemetry.addEvent("show.1", "dialog", null, "prompt_device_selection");
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationDevicePrompt]);
