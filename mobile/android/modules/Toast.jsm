/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

this.EXPORTED_SYMBOLS = ["Toast"];

// Copied from browser.js
// TODO: We should move this method to a common importable location
function resolveGeckoURI(uri) {
  if (!uri)
    throw "Can't resolve an empty uri";

  if (uri.startsWith("chrome://")) {
    let registry = Cc['@mozilla.org/chrome/chrome-registry;1'].getService(Ci["nsIChromeRegistry"]);
    return registry.convertChromeURL(Services.io.newURI(uri, null, null)).spec;
  } else if (uri.startsWith("resource://")) {
    let handler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    return handler.resolveURI(Services.io.newURI(uri, null, null));
  }

  return uri;
}

var Toast = {
  LONG: "long",
  SHORT: "short",

  show: function(message, duration, options) {
    let msg = {
      type: "Toast:Show",
      message: message,
      duration: duration
    };

    let callback;
    if (options && options.button) {
      msg.button = { };

      // null is badly handled by the receiver, so try to avoid including nulls.
      if (options.button.label) {
        msg.button.label = options.button.label;
      }

      if (options.button.icon) {
        // If the caller specified a button, make sure we convert any chrome urls
        // to jar:jar urls so that the frontend can show them
        msg.button.icon = resolveGeckoURI(options.button.icon);
      };

      callback = options.button.callback;
    }

    sendMessageToJava(msg, callback);
  }
}
