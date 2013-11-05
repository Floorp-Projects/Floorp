// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Home"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

// See bug 915424
function resolveGeckoURI(aURI) {
  if (!aURI)
    throw "Can't resolve an empty uri";

  if (aURI.startsWith("chrome://")) {
    let registry = Cc['@mozilla.org/chrome/chrome-registry;1'].getService(Ci["nsIChromeRegistry"]);
    return registry.convertChromeURL(Services.io.newURI(aURI, null, null)).spec;
  } else if (aURI.startsWith("resource://")) {
    let handler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    return handler.resolveURI(Services.io.newURI(aURI, null, null));
  }
  return aURI;
}

function sendMessageToJava(message) {
  return Services.androidBridge.handleGeckoMessage(JSON.stringify(message));
}

function BannerMessage(options) {
  let uuidgen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  this.id = uuidgen.generateUUID().toString();

  if ("text" in options && options.text != null)
    this.text = options.text;

  if ("icon" in options && options.icon != null)
    this.iconURI = resolveGeckoURI(options.icon);

  if ("onshown" in options && typeof options.onshown === "function")
    this.onshown = options.onshown;

  if ("onclick" in options && typeof options.onclick === "function")
    this.onclick = options.onclick;
}

let HomeBanner = {
  // Holds the messages that will rotate through the banner.
  _messages: {},

  // A queue used to keep track of which message to show next.
  _queue: [],

  observe: function(subject, topic, data) {
    switch(topic) {
      case "HomeBanner:Get":
        this._handleGet();
        break;

      case "HomeBanner:Click":
        this._handleClick(data);
        break;
    }
  },

  _handleGet: function() {
    // Get the message from the front of the queue, then add it back
    // to the end of the queue to show it again later.
    let id = this._queue.shift();
    this._queue.push(id);

    let message = this._messages[id];
    sendMessageToJava({
      type: "HomeBanner:Data",
      id: message.id,
      text: message.text,
      iconURI: message.iconURI
    });

    if (message.onshown)
      message.onshown();
  },

  _handleClick: function(id) {
    let message = this._messages[id];
    if (message.onclick)
      message.onclick();
  },

  /**
   * Adds a new banner message to the rotation.
   *
   * @return id Unique identifer for the message.
   */
  add: function(options) {
    let message = new BannerMessage(options);
    this._messages[message.id] = message;

    // Add the new message to the end of the queue.
    this._queue.push(message.id);

    // If this is the first message we're adding, add
    // observers to listen for requests from the Java UI.
    if (Object.keys(this._messages).length == 1) {
      Services.obs.addObserver(this, "HomeBanner:Get", false);
      Services.obs.addObserver(this, "HomeBanner:Click", false);

      // Send a message to Java, in case there's an active HomeBanner
      // waiting for a response.
      this._handleGet();
    }

    return message.id;
  },

  /**
   * Removes a banner message from the rotation.
   *
   * @param id The id of the message to remove.
   */
  remove: function(id) {
    delete this._messages[id];

    // Remove the message from the queue.
    let index = this._queue.indexOf(id);
    this._queue.splice(index, 1);

    // If there are no more messages, remove the observers.
    if (Object.keys(this._messages).length == 0) {
      Services.obs.removeObserver(this, "HomeBanner:Get");
      Services.obs.removeObserver(this, "HomeBanner:Click");
    }
  }
};

// Public API
this.Home = {
  banner: HomeBanner
}
