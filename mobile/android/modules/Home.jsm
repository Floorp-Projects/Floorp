// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Home"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SharedPreferences.jsm");

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

function Panel(options) {
  if ("id" in options)
    this.id = options.id;

  if ("title" in options)
    this.title = options.title;

  if ("layout" in options)
    this.layout = options.layout;

  if ("views" in options)
    this.views = options.views;
}

let HomePanels = {
  // Valid layouts for a panel.
  Layout: {
    FRAME: "frame"
  },

  // Valid types of views for a dataset.
  View: {
    LIST: "list"
  },

  // Holds the currrent set of registered panels.
  _panels: {},

  _handleGet: function(requestId) {
    let panels = [];
    for (let id in this._panels) {
      let panel = this._panels[id];
      panels.push({
        id: panel.id,
        title: panel.title,
        layout: panel.layout,
        views: panel.views
      });
    }

    sendMessageToJava({
      type: "HomePanels:Data",
      panels: panels,
      requestId: requestId
    });
  },

  add: function(options) {
    let panel = new Panel(options);
    if (!panel.id || !panel.title) {
      throw "Home.panels: Can't create a home panel without an id and title!";
    }

    // Bail if the panel already exists
    if (panel.id in this._panels) {
      throw "Home.panels: Panel already exists: id = " + panel.id;
    }

    if (!this._valueExists(this.Layout, panel.layout)) {
      throw "Home.panels: Invalid layout for panel: panel.id = " + panel.id + ", panel.layout =" + panel.layout;
    }

    for (let view of panel.views) {
      if (!this._valueExists(this.View, view.type)) {
        throw "Home.panels: Invalid view type: panel.id = " + panel.id + ", view.type = " + view.type;
      }

      if (!view.dataset) {
        throw "Home.panels: No dataset provided for view: panel.id = " + panel.id + ", view.type = " + view.type;
      }
    }

    this._panels[panel.id] = panel;
  },

  remove: function(id) {
    delete this._panels[id];

    sendMessageToJava({
      type: "HomePanels:Remove",
      id: id
    });
  },

  // Helper function used to see if a value is in an object.
  _valueExists: function(obj, value) {
    for (let key in obj) {
      if (obj[key] == value) {
        return true;
      }
    }
    return false;
  }
};

// Public API
this.Home = {
  banner: HomeBanner,
  panels: HomePanels,

  // Lazy notification observer registered in browser.js
  observe: function(subject, topic, data) {
    switch(topic) {
      case "HomePanels:Get":
        HomePanels._handleGet(data);
        break;
    }
  }
}
