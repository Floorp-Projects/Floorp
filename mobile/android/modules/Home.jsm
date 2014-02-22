// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Home"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SharedPreferences.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

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

  if ("ondismiss" in options && typeof options.ondismiss === "function")
    this.ondismiss = options.ondismiss;
}

let HomeBanner = (function () {
  // Holds the messages that will rotate through the banner.
  let _messages = {};

  // A queue used to keep track of which message to show next.
  let _queue = [];


  let _handleGet = function() {
    // Get the message from the front of the queue, then add it back
    // to the end of the queue to show it again later.
    let id = _queue.shift();
    _queue.push(id);

    let message = _messages[id];
    sendMessageToJava({
      type: "HomeBanner:Data",
      id: message.id,
      text: message.text,
      iconURI: message.iconURI
    });

    if (message.onshown)
      message.onshown();
  };

  let _handleClick = function(id) {
    let message = _messages[id];
    if (message.onclick)
      message.onclick();
  };

  let _handleDismiss = function(id) {
    let message = _messages[id];
    if (message.ondismiss)
      message.ondismiss();
  };

  return Object.freeze({
    observe: function(subject, topic, data) {
      switch(topic) {
        case "HomeBanner:Get":
          _handleGet();
          break;

        case "HomeBanner:Click":
          _handleClick(data);
          break;

        case "HomeBanner:Dismiss":
          _handleDismiss(data);
          break; 
      }
    },

    /**
     * Adds a new banner message to the rotation.
     *
     * @return id Unique identifer for the message.
     */
    add: function(options) {
      let message = new BannerMessage(options);
      _messages[message.id] = message;

      // Add the new message to the end of the queue.
      _queue.push(message.id);

      // If this is the first message we're adding, add
      // observers to listen for requests from the Java UI.
      if (Object.keys(_messages).length == 1) {
        Services.obs.addObserver(this, "HomeBanner:Get", false);
        Services.obs.addObserver(this, "HomeBanner:Click", false);
        Services.obs.addObserver(this, "HomeBanner:Dismiss", false);

        // Send a message to Java, in case there's an active HomeBanner
        // waiting for a response.
        _handleGet();
      }

      return message.id;
    },

    /**
     * Removes a banner message from the rotation.
     *
     * @param id The id of the message to remove.
     */
    remove: function(id) {
      if (!(id in _messages)) {
        throw "Home.banner: Can't remove message that doesn't exist: id = " + id;
      }

      delete _messages[id];

      // Remove the message from the queue.
      let index = _queue.indexOf(id);
      _queue.splice(index, 1);

      // If there are no more messages, remove the observers.
      if (Object.keys(_messages).length == 0) {
        Services.obs.removeObserver(this, "HomeBanner:Get");
        Services.obs.removeObserver(this, "HomeBanner:Click");
        Services.obs.removeObserver(this, "HomeBanner:Dismiss");
      }
    }
  });
})();

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

let HomePanels = (function () {
  // Holds the currrent set of registered panels.
  let _panels = {};

  let _panelToJSON = function(panel) {
    return {
      id: panel.id,
      title: panel.title,
      layout: panel.layout,
      views: panel.views
    };
  };

  let _handleGet = function(data) {
    let requestId = data.requestId;
    let ids = data.ids || null;

    let panels = [];
    for (let id in _panels) {
      let panel = _panels[id];

      // Null ids means we want to fetch all available panels
      if (ids == null || ids.indexOf(panel.id) >= 0) {
        panels.push(_panelToJSON(panel));
      }
    }

    sendMessageToJava({
      type: "HomePanels:Data",
      panels: panels,
      requestId: requestId
    });
  };

  // Helper function used to see if a value is in an object.
  let _valueExists = function(obj, value) {
    for (let key in obj) {
      if (obj[key] == value) {
        return true;
      }
    }
    return false;
  };

  let _assertPanelExists = function(id) {
    if (!(id in _panels)) {
      throw "Home.panels: Panel doesn't exist: id = " + id;
    }
  };

  return Object.freeze({
    // Valid layouts for a panel.
    Layout: Object.freeze({
      FRAME: "frame"
    }),

    // Valid types of views for a dataset.
    View: Object.freeze({
      LIST: "list",
      GRID: "grid"
    }),

    // Valid item types for a panel view.
    Item: Object.freeze({
      ARTICLE: "article",
      IMAGE: "image"
    }),

    // Valid item handlers for a panel view.
    ItemHandler: Object.freeze({
      BROWSER: "browser",
      INTENT: "intent"
    }),

    register: function(options) {
      let panel = new Panel(options);

      // Bail if the panel already exists
      if (panel.id in _panels) {
        throw "Home.panels: Panel already exists: id = " + panel.id;
      }

      if (!panel.id || !panel.title) {
        throw "Home.panels: Can't create a home panel without an id and title!";
      }

      if (!_valueExists(this.Layout, panel.layout)) {
        throw "Home.panels: Invalid layout for panel: panel.id = " + panel.id + ", panel.layout =" + panel.layout;
      }

      for (let view of panel.views) {
        if (!_valueExists(this.View, view.type)) {
          throw "Home.panels: Invalid view type: panel.id = " + panel.id + ", view.type = " + view.type;
        }

        if (!view.itemType) {
          if (view.type == this.View.LIST) {
            // Use ARTICLE item type by default in LIST views
            view.itemType = this.Item.ARTICLE;
          } else if (view.type == this.View.GRID) {
            // Use IMAGE item type by default in GRID views
            view.itemType = this.Item.IMAGE;
          }
        } else if (!_valueExists(this.Item, view.itemType)) {
          throw "Home.panels: Invalid item type: panel.id = " + panel.id + ", view.itemType = " + view.itemType;
        }

        if (!view.itemHandler) {
          // Use BROWSER item handler by default
          view.itemHandler = this.ItemHandler.BROWSER;
        } else if (!_valueExists(this.ItemHandler, view.itemHandler)) {
          throw "Home.panels: Invalid item handler: panel.id = " + panel.id + ", view.itemHandler = " + view.itemHandler;
        }

        if (!view.dataset) {
          throw "Home.panels: No dataset provided for view: panel.id = " + panel.id + ", view.type = " + view.type;
        }
      }

      _panels[panel.id] = panel;
    },

    unregister: function(id) {
      _assertPanelExists(id);

      delete _panels[id];
    },

    install: function(id) {
      _assertPanelExists(id);

      sendMessageToJava({
        type: "HomePanels:Install",
        panel: _panelToJSON(_panels[id])
      });
    },

    uninstall: function(id) {
      _assertPanelExists(id);

      sendMessageToJava({
        type: "HomePanels:Uninstall",
        id: id
      });
    },

    update: function(id) {
      _assertPanelExists(id);

      sendMessageToJava({
        type: "HomePanels:Update",
        panel: _panelToJSON(_panels[id])
      });
    }
  });
})();

// Public API
this.Home = Object.freeze({
  banner: HomeBanner,
  panels: HomePanels,

  // Lazy notification observer registered in browser.js
  observe: function(subject, topic, data) {
    switch(topic) {
      case "HomePanels:Get":
        HomePanels._handleGet(JSON.parse(data));
        break;
    }
  }
});
