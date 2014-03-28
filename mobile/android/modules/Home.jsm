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

// Keep this in sync with the constant defined in PanelAuthCache.java
const PREFS_PANEL_AUTH_PREFIX = "home_panels_auth_";

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


  let _handleGet = function() {
    // Choose a message at random.
    let keys = Object.keys(_messages);
    let randomId = keys[Math.floor(Math.random() * keys.length)];
    let message = _messages[randomId];

    sendMessageToJava({
      type: "HomeBanner:Data",
      id: message.id,
      text: message.text,
      iconURI: message.iconURI
    });
  };

  let _handleShown = function(id) {
    let message = _messages[id];
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

        case "HomeBanner:Shown":
          _handleShown(data);
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

      // If this is the first message we're adding, add
      // observers to listen for requests from the Java UI.
      if (Object.keys(_messages).length == 1) {
        Services.obs.addObserver(this, "HomeBanner:Get", false);
        Services.obs.addObserver(this, "HomeBanner:Shown", false);
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

      // If there are no more messages, remove the observers.
      if (Object.keys(_messages).length == 0) {
        Services.obs.removeObserver(this, "HomeBanner:Get");
        Services.obs.removeObserver(this, "HomeBanner:Shown");
        Services.obs.removeObserver(this, "HomeBanner:Click");
        Services.obs.removeObserver(this, "HomeBanner:Dismiss");
      }
    }
  });
})();

// We need this function to have access to the HomePanels
// private members without leaking it outside Home.jsm.
let handlePanelsGet;
let handlePanelsAuthenticate;

let HomePanels = (function () {
  // Holds the current set of registered panels that can be
  // installed, updated, uninstalled, or unregistered. It maps
  // panel ids with the functions that dynamically generate
  // their respective panel options. This is used to retrieve
  // the current list of available panels in the system.
  // See HomePanels:Get handler.
  let _registeredPanels = {};

  // Valid layouts for a panel.
  let Layout = Object.freeze({
    FRAME: "frame"
  });

  // Valid types of views for a dataset.
  let View = Object.freeze({
    LIST: "list",
    GRID: "grid"
  });

  // Valid item types for a panel view.
  let Item = Object.freeze({
    ARTICLE: "article",
    IMAGE: "image"
  });

  // Valid item handlers for a panel view.
  let ItemHandler = Object.freeze({
    BROWSER: "browser",
    INTENT: "intent"
  });

  function Panel(id, options) {
    this.id = id;
    this.title = options.title;
    this.layout = options.layout;
    this.views = options.views;

    if (!this.id || !this.title) {
      throw "Home.panels: Can't create a home panel without an id and title!";
    }

    if (!this.layout) {
      // Use FRAME layout by default
      this.layout = Layout.FRAME;
    } else if (!_valueExists(Layout, this.layout)) {
      throw "Home.panels: Invalid layout for panel: panel.id = " + this.id + ", panel.layout =" + this.layout;
    }

    for (let view of this.views) {
      if (!_valueExists(View, view.type)) {
        throw "Home.panels: Invalid view type: panel.id = " + this.id + ", view.type = " + view.type;
      }

      if (!view.itemType) {
        if (view.type == View.LIST) {
          // Use ARTICLE item type by default in LIST views
          view.itemType = Item.ARTICLE;
        } else if (view.type == View.GRID) {
          // Use IMAGE item type by default in GRID views
          view.itemType = Item.IMAGE;
        }
      } else if (!_valueExists(Item, view.itemType)) {
        throw "Home.panels: Invalid item type: panel.id = " + this.id + ", view.itemType = " + view.itemType;
      }

      if (!view.itemHandler) {
        // Use BROWSER item handler by default
        view.itemHandler = ItemHandler.BROWSER;
      } else if (!_valueExists(ItemHandler, view.itemHandler)) {
        throw "Home.panels: Invalid item handler: panel.id = " + this.id + ", view.itemHandler = " + view.itemHandler;
      }

      if (!view.dataset) {
        throw "Home.panels: No dataset provided for view: panel.id = " + this.id + ", view.type = " + view.type;
      }
    }

    if (options.authHandler) {
      if (!options.authHandler.messageText) {
        throw "Home.panels: Invalid authHandler messageText: panel.id = " + this.id;
      }
      if (!options.authHandler.buttonText) {
        throw "Home.panels: Invalid authHandler buttonText: panel.id = " + this.id;
      }

      this.authConfig = {
        messageText: options.authHandler.messageText,
        buttonText: options.authHandler.buttonText
      };

      // Include optional image URL if it is specified.
      if (options.authHandler.imageUrl) {
        this.authConfig.imageUrl = options.authHandler.imageUrl;
      }
    }
  }

  let _generatePanel = function(id) {
    let options = _registeredPanels[id]();
    return new Panel(id, options);
  };

  handlePanelsGet = function(data) {
    let requestId = data.requestId;
    let ids = data.ids || null;

    let panels = [];
    for (let id in _registeredPanels) {
      // Null ids means we want to fetch all available panels
      if (ids == null || ids.indexOf(id) >= 0) {
        try {
          panels.push(_generatePanel(id));
        } catch(e) {
          Cu.reportError("Home.panels: Invalid options, panel.id = " + id + ": " + e);
        }
      }
    }

    sendMessageToJava({
      type: "HomePanels:Data",
      panels: panels,
      requestId: requestId
    });
  };

  handlePanelsAuthenticate = function(id) {
    // Generate panel options to get auth handler.
    let options = _registeredPanels[id]();
    if (!options.authHandler) {
      throw "Home.panels: Invalid authHandler for panel.id = " + id;
    }
    if (!options.authHandler.authenticate || typeof options.authHandler.authenticate !== "function") {
      throw "Home.panels: Invalid authHandler authenticate function: panel.id = " + this.id;
    }
    options.authHandler.authenticate();
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
    if (!(id in _registeredPanels)) {
      throw "Home.panels: Panel doesn't exist: id = " + id;
    }
  };

  return Object.freeze({
    Layout: Layout,
    View: View,
    Item: Item,
    ItemHandler: ItemHandler,

    register: function(id, optionsCallback) {
      // Bail if the panel already exists
      if (id in _registeredPanels) {
        throw "Home.panels: Panel already exists: id = " + id;
      }

      if (!optionsCallback || typeof optionsCallback !== "function") {
        throw "Home.panels: Panel callback must be a function: id = " + id;
      }

      _registeredPanels[id] = optionsCallback;
    },

    unregister: function(id) {
      _assertPanelExists(id);

      delete _registeredPanels[id];
    },

    install: function(id) {
      _assertPanelExists(id);

      sendMessageToJava({
        type: "HomePanels:Install",
        panel: _generatePanel(id)
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
        panel: _generatePanel(id)
      });
    },

    setAuthenticated: function(id, isAuthenticated) {
      _assertPanelExists(id);

      let authKey = PREFS_PANEL_AUTH_PREFIX + id;
      let sharedPrefs = new SharedPreferences();
      sharedPrefs.setBoolPref(authKey, isAuthenticated);
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
        handlePanelsGet(JSON.parse(data));
        break;
      case "HomePanels:Authenticate":
        handlePanelsAuthenticate(data);
        break;
    }
  }
});
