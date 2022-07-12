/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ShareDelegate"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.jsm",
});

const domBundle = Services.strings.createBundle(
  "chrome://global/locale/dom/dom.properties"
);

const { debug, warn } = GeckoViewUtils.initLogging("ShareDelegate");

class ShareDelegate {
  init(aParent) {
    this._openerWindow = aParent;
  }

  get openerWindow() {
    return this._openerWindow;
  }

  async share(aTitle, aText, aUri) {
    const ABORT = 2;
    const FAILURE = 1;
    const SUCCESS = 0;

    const msg = {
      type: "share",
      title: aTitle,
      text: aText,
      uri: aUri ? aUri.displaySpec : null,
    };
    const prompt = new lazy.GeckoViewPrompter(this._openerWindow);
    const result = await new Promise(resolve => {
      prompt.asyncShowPrompt(msg, resolve);
    });

    if (!result) {
      // A null result is treated as a dismissal in GeckoViewPrompter.
      throw new DOMException(
        domBundle.GetStringFromName("WebShareAPI_Aborted"),
        "AbortError"
      );
    }

    const res = result && result.response;
    switch (res) {
      case FAILURE:
        throw new DOMException(
          domBundle.GetStringFromName("WebShareAPI_Failed"),
          "DataError"
        );
      case ABORT: // Handle aborted attempt and invalid responses the same.
        throw new DOMException(
          domBundle.GetStringFromName("WebShareAPI_Aborted"),
          "AbortError"
        );
      case SUCCESS:
        return;
      default:
        throw new DOMException("Unknown error.", "UnknownError");
    }
  }
}

ShareDelegate.prototype.classID = Components.ID(
  "{1201d357-8417-4926-a694-e6408fbedcf8}"
);
ShareDelegate.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsISharePicker",
]);
