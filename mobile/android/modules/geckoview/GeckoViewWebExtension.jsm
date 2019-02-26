/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewWebExtension"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {GeckoViewUtils} = ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Extension: "resource://gre/modules/Extension.jsm",
});

XPCOMUtils.defineLazyGetter(this, "require", () => {
  const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
  return require;
});

XPCOMUtils.defineLazyGetter(this, "Services", () => {
  const Services = require("Services");
  return Services;
});

const {debug, warn} = GeckoViewUtils.initLogging("Console"); // eslint-disable-line no-unused-vars

var GeckoViewWebExtension = {
  async registerWebExtension(aId, aUri, aCallback) {
    const params = {
      id: aId,
      resourceURI: aUri,
      temporarilyInstalled: true,
      builtIn: true,
    };

    let file;
    if (aUri instanceof Ci.nsIFileURL) {
      file = aUri.file;
    }

    try {
      await Extension.getBootstrapScope(aId, file)
          .startup(params, undefined);
    } catch (ex) {
      aCallback.onError(`Error registering WebExtension at: ${aUri.spec}. ${ex}`);
      return;
    }

    aCallback.onSuccess();
  },

  onEvent(aEvent, aData, aCallback) {
    debug `onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:RegisterWebExtension": {
        const uri = Services.io.newURI(aData.locationUri);
        if (uri == null || (!(uri instanceof Ci.nsIFileURL) &&
              !(uri instanceof Ci.nsIJARURI))) {
          aCallback.onError(`Extension does not point to a resource URI or a file URL. extension=${aData.locationUri}`);
          return;
        }

        if (uri.fileName != "" && uri.fileExtension != "xpi") {
          aCallback.onError(`Extension does not point to a folder or an .xpi file. Hint: the path needs to end with a "/" to be considered a folder. extension=${aData.locationUri}`);
          return;
        }

        this.registerWebExtension(aData.id, uri, aCallback);
      }
    }
  },
};
