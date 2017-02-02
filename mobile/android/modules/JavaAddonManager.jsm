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
    return registry.convertChromeURL(Services.io.newURI(uri)).spec;
  } else if (uri.startsWith("resource://")) {
    let handler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    return handler.resolveURI(Services.io.newURI(uri));
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
    return EventDispatcher.instance.sendRequestForResult({
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

  this._listeners = {
    list: {},
    onEvent: function(event, data, callback) {
      let listener = this.list[event];
      if (listener) {
        let ret = listener(data);
        callback && callback.onSuccess(ret);
      } else {
        callback && callback.onError("No listener");
      }
    },
  };
}

JavaAddonV1.prototype = Object.freeze({
  unload: function() {
    if (!this._loaded) {
      return;
    }

    EventDispatcher.instance.sendRequestForResult({
      type: "JavaAddonManagerV1:Unload",
      guid: this._guid
    })
      .then(() => {
        this._loaded = false;
        for (let event in this._listeners.list) {
          // If we use this.removeListener, we prefix twice.
          EventDispatcher.instance.unregisterListener(this._listeners, event);
        }
        this._listeners.list = {};
      });
  },

  _prefix: function(message) {
    let newMessage = Cu.cloneInto(message, {}, { cloneFunctions: false });
    newMessage.type = this._guid + ":" + message.type;
    return newMessage;
  },

  sendRequest: function(message) {
    return EventDispatcher.instance.sendRequest(this._prefix(message));
  },

  sendRequestForResult: function(message) {
    return EventDispatcher.instance.sendRequestForResult(this._prefix(message)).then(
        wrapper => wrapper.response, wrapper => { throw wrapper && wrapper.response; });
  },

  addListener: function(listener, message) {
    let prefixedMessage = this._guid + ":" + message;
    this._listeners.list[prefixedMessage] = listener;
    return EventDispatcher.instance.registerListener(this._listeners, prefixedMessage);
  },

  removeListener: function(message) {
    let prefixedMessage = this._guid + ":" + message;
    delete this._listeners.list[prefixedMessage];
    return EventDispatcher.instance.unregisterListener(this._listeners, prefixedMessage);
  }
});
