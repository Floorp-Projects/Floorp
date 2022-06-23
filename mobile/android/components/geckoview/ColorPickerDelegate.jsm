/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ColorPickerDelegate"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.jsm",
});

const { debug, warn } = GeckoViewUtils.initLogging("ColorPickerDelegate");

class ColorPickerDelegate {
  init(aParent, aTitle, aInitialColor) {
    this._prompt = new lazy.GeckoViewPrompter(aParent);
    this._msg = {
      type: "color",
      title: aTitle,
      value: aInitialColor,
    };
  }

  open(aColorPickerShownCallback) {
    this._prompt.asyncShowPrompt(this._msg, result => {
      // OK: result
      // Cancel: !result
      aColorPickerShownCallback.done((result && result.color) || "");
    });
  }
}

ColorPickerDelegate.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIColorPicker",
]);
