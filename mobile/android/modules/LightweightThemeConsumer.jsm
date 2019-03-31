/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {LightweightThemeManager} = ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm");

ChromeUtils.defineModuleGetter(this, "EventDispatcher",
                               "resource://gre/modules/Messaging.jsm");
ChromeUtils.defineModuleGetter(this, "LightweightThemePersister",
  "resource://gre/modules/addons/LightweightThemePersister.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  Services.obs.addObserver(this, "lightweight-theme-styling-update");
  Services.obs.addObserver(this, "lightweight-theme-apply");

  this._update(LightweightThemeManager.currentThemeWithFallback);
}

LightweightThemeConsumer.prototype = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "lightweight-theme-styling-update") {
      this._update(aSubject.wrappedJSObject.theme);
    } else if (aTopic == "lightweight-theme-apply") {
      this._update(LightweightThemeManager.currentThemeWithFallback);
    }
  },

  destroy: function() {
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
    Services.obs.removeObserver(this, "lightweight-theme-apply");
    this._doc = null;
  },

  _update: function(aData) {
    let active = aData && aData.id !== DEFAULT_THEME_ID;
    let msg = active ? { type: "LightweightTheme:Update" } :
                       { type: "LightweightTheme:Disable" };

    if (active) {
      LightweightThemePersister.persistImages(aData, () => {
        msg.data = LightweightThemePersister.getPersistedData(aData);
        EventDispatcher.instance.sendRequest(msg);
      });
    } else {
      EventDispatcher.instance.sendRequest(msg);
    }
  },
};
