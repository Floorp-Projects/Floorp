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
  Services.obs.addObserver(this, "lightweight-theme-apply", false);

  this._update(LightweightThemeManager.currentThemeForDisplay);
}

LightweightThemeConsumer.prototype = {
  observe: function (aSubject, aTopic, aData) {
    if (aTopic == "lightweight-theme-styling-update")
      this._update(JSON.parse(aData));
    else if (aTopic == "lightweight-theme-apply")
      this._update(LightweightThemeManager.currentThemeForDisplay);
  },

  destroy: function () {
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
    Services.obs.removeObserver(this, "lightweight-theme-apply");
    this._doc = null;
  },

  _update: function (aData) {
    if (!aData)
      aData = { headerURL: "", footerURL: "", textcolor: "", accentcolor: "" };

    let active = !!aData.headerURL;

    let msg = active ? { type: "LightweightTheme:Update", data: aData } :
                       { type: "LightweightTheme:Disable" };
    Services.androidBridge.handleGeckoMessage(JSON.stringify(msg));
  }
}
