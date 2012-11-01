/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];
let Cc = Components.classes;
let Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");

function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  Services.obs.addObserver(this, "lightweight-theme-styling-update", false);

  this._update(LightweightThemeManager.currentThemeForDisplay);
}

LightweightThemeConsumer.prototype = {
  observe: function (aSubject, aTopic, aData) {
    if (aTopic != "lightweight-theme-styling-update")
      return;

    this._update(JSON.parse(aData));
  },

  destroy: function () {
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
    this._doc = null;
  },

  _update: function (aData) {
    if (!aData)
      aData = { headerURL: "", footerURL: "", textcolor: "", accentcolor: "" };

    let active = !!aData.headerURL;

    let msg = active ? { gecko: { type: "LightweightTheme:Update", data: aData } } :
                       { gecko: { type: "LightweightTheme:Disable" } };
    let bridge = Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge);
    bridge.handleGeckoMessage(JSON.stringify(msg));
  }
}
