// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["JavaAddonManager"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components; /*global Components */

Cu.import("resource://gre/modules/Messaging.jsm"); /*global Messaging */
Cu.import("resource://gre/modules/Services.jsm"); /*global Services */

function resolveGeckoURI(uri) {
  if (!uri) {
    throw new Error("Can't resolve an empty uri");
  }
  if (uri.startsWith("chrome://")) {
    let registry = Cc['@mozilla.org/chrome/chrome-registry;1'].getService(Ci["nsIChromeRegistry"]);
    return registry.convertChromeURL(Services.io.newURI(uri, null, null)).spec;
  } else if (uri.startsWith("resource://")) {
    let handler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    return handler.resolveURI(Services.io.newURI(uri, null, null));
  }
  return uri;
}

/**
 * A promise-based API
 */
var JavaAddonManager = Object.freeze({
  classInstanceFromFile: function(classname, filename) {
    if (!classname) {
      throw new Error("classname cannot be null");
    }
    if (!filename) {
      throw new Error("filename cannot be null");
    }
    return Messaging.sendRequestForResult({
      type: "JavaAddonManagerV1:Load",
      classname: classname,
      filename: resolveGeckoURI(filename)
    })
      .then((guid) => {
        if (!guid) {
          throw new Error("Internal error: guid should not be null");
        }
        return new JavaAddonV1({classname: classname, guid: guid});
      });
  }
});

function JavaAddonV1(options = {}) {
  if (!(this instanceof JavaAddonV1)) {
    return new JavaAddonV1(options);
  }
  if (!options.classname) {
    throw new Error("options.classname cannot be null");
  }
  if (!options.guid) {
    throw new Error("options.guid cannot be null");
  }
  this._classname = options.classname;
  this._guid = options.guid;
  this._loaded = true;
  this._listeners = {};
}

JavaAddonV1.prototype = Object.freeze({
  unload: function() {
    if (!this._loaded) {
      return;
    }

    Messaging.sendRequestForResult({
      type: "JavaAddonManagerV1:Unload",
      guid: this._guid
    })
      .then(() => {
        this._loaded = false;
        for (let listener of this._listeners) {
          // If we use this.removeListener, we prefix twice.
          Messaging.removeListener(listener);
        }
        this._listeners = {};
      });
  },

  _prefix: function(message) {
    let newMessage = Cu.cloneInto(message, {}, { cloneFunctions: false });
    newMessage.type = this._guid + ":" + message.type;
    return newMessage;
  },

  sendRequest: function(message) {
    return Messaging.sendRequest(this._prefix(message));
  },

  sendRequestForResult: function(message) {
    return Messaging.sendRequestForResult(this._prefix(message));
  },

  addListener: function(listener, message) {
    let prefixedMessage = this._guid + ":" + message;
    this._listeners[prefixedMessage] = listener;
    return Messaging.addListener(listener, prefixedMessage);
  },

  removeListener: function(message) {
    let prefixedMessage = this._guid + ":" + message;
    delete this._listeners[prefixedMessage];
    return Messaging.removeListener(prefixedMessage);
  }
});
