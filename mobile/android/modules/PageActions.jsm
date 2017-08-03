/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

this.EXPORTED_SYMBOLS = ["PageActions"];

// Copied from browser.js
// TODO: We should move this method to a common importable location
function resolveGeckoURI(aURI) {
  if (!aURI)
    throw "Can't resolve an empty uri";

  if (aURI.startsWith("chrome://")) {
    let registry = Cc['@mozilla.org/chrome/chrome-registry;1'].getService(Ci.nsIChromeRegistry);
    return registry.convertChromeURL(Services.io.newURI(aURI)).spec;
  } else if (aURI.startsWith("resource://")) {
    let handler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    return handler.resolveURI(Services.io.newURI(aURI));
  }
  return aURI;
}

var PageActions = {
  _items: { },

  _initialized: false,

  _maybeInitialize: function() {
    if (!this._initialized && Object.keys(this._items).length) {
      this._initialized = true;
      EventDispatcher.instance.registerListener(this, [
        "PageActions:Clicked",
        "PageActions:LongClicked",
      ]);
    }
  },

  _maybeUninitialize: function() {
    if (this._initialized && !Object.keys(this._items).length) {
      this._initialized = false;
      EventDispatcher.instance.unregisterListener(this, [
        "PageActions:Clicked",
        "PageActions:LongClicked",
      ]);
    }
  },

  onEvent: function(event, data, callback) {
    let item = this._items[data.id];
    if (event == "PageActions:Clicked") {
      if (item.clickCallback) {
        item.clickCallback();
      }
    } else if (event == "PageActions:LongClicked") {
      if (item.longClickCallback) {
        item.longClickCallback();
      }
    }
  },

  isShown: function(id) {
    return !!this._items[id];
  },

  synthesizeClick: function(id) {
    let item = this._items[id];
    if (item && item.clickCallback) {
      item.clickCallback();
    }
  },

  add: function(aOptions) {
    let id = aOptions.id || uuidgen.generateUUID().toString()

    EventDispatcher.instance.sendRequest({
      type: "PageActions:Add",
      id: id,
      title: aOptions.title,
      icon: resolveGeckoURI(aOptions.icon),
      important: "important" in aOptions ? aOptions.important : false
    });

    this._items[id] = {};

    if (aOptions.clickCallback) {
      this._items[id].clickCallback = aOptions.clickCallback;
    }

    if (aOptions.longClickCallback) {
      this._items[id].longClickCallback = aOptions.longClickCallback;
    }

    this._maybeInitialize();
    return id;
  },

  remove: function(id) {
    EventDispatcher.instance.sendRequest({
      type: "PageActions:Remove",
      id: id
    });

    delete this._items[id];
    this._maybeUninitialize();
  }
}
