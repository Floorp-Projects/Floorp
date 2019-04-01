/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {LightweightThemeManager} = ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm");
const {ExtensionUtils} = ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

ChromeUtils.defineModuleGetter(this, "EventDispatcher",
                               "resource://gre/modules/Messaging.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

let RESOLVE_PROPERTIES = ["headerURL"];

let handlers = new ExtensionUtils.DefaultMap(proto => {
  try {
    return Cc[`@mozilla.org/network/protocol;1?name=${proto}`]
      .getService(Ci.nsISubstitutingProtocolHandler);
  } catch (e) {
    return null;
  }
});

// The Java front-end code cannot understand internal protocols like
// resource:, so resolve them to their underlying file: or jar: URIs
// when possible.
function maybeResolveURL(url) {
  try {
    let uri = Services.io.newURI(url);
    let handler = handlers.get(uri.scheme);
    if (handler) {
      return handler.resolveURI(uri);
    }
  } catch (e) {
    Cu.reportError(e);
  }
  return url;
}

class LightweightThemeConsumer {
  constructor(aDocument) {
    this._doc = aDocument;
    Services.obs.addObserver(this, "lightweight-theme-styling-update");

    this._update(LightweightThemeManager.currentThemeWithFallback);
  }

  observe(aSubject, aTopic, aData) {
    if (aTopic == "lightweight-theme-styling-update") {
      this._update(aSubject.wrappedJSObject.theme);
    }
  }

  destroy() {
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
    this._doc = null;
  }

  _update(aData) {
    let active = aData && aData.id !== DEFAULT_THEME_ID;
    let msg = { type: (active ? "LightweightTheme:Update"
                              : "LightweightTheme:Disable") };

    if (active) {
      msg.data = {...aData};
      for (let prop of RESOLVE_PROPERTIES) {
        if (msg.data[prop]) {
          msg.data[prop] = maybeResolveURL(msg.data[prop]);
        }
      }
    }
    EventDispatcher.instance.sendRequest(msg);
  }
}
