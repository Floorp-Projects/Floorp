/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm");

ChromeUtils.defineModuleGetter(this, "EventDispatcher",
                               "resource://gre/modules/Messaging.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  Services.obs.addObserver(this, "lightweight-theme-styling-update");
  Services.obs.addObserver(this, "lightweight-theme-apply");

  this._update(LightweightThemeManager.currentThemeForDisplay);
}

LightweightThemeConsumer.prototype = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "lightweight-theme-styling-update") {
      let parsedData = JSON.parse(aData);
      if (!parsedData) {
        parsedData = { theme: null };
      }
      this._update(parsedData.theme);
    } else if (aTopic == "lightweight-theme-apply") {
      this._update(LightweightThemeManager.currentThemeForDisplay);
    }
  },

  destroy: function() {
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
    Services.obs.removeObserver(this, "lightweight-theme-apply");
    this._doc = null;
  },

  _update: function(aData) {
    let active = aData && aData.id !== DEFAULT_THEME_ID;
    if (!aData) {
      aData = {};
    }

    let msg = active ? { type: "LightweightTheme:Update", data: aData } :
                       { type: "LightweightTheme:Disable" };
    EventDispatcher.instance.sendRequest(msg);
  },
};
