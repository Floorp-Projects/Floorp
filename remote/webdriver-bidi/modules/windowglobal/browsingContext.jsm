/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["browsingContext"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  LoadListener: "chrome://remote/content/shared/listeners/LoadListener.jsm",
});

class BrowsingContextModule extends Module {
  #listeningToDOMContentLoaded;
  #listeningToLoad;
  #loadListener;

  constructor(messageHandler) {
    super(messageHandler);

    // Setup the LoadListener as early as possible.
    this.#loadListener = new lazy.LoadListener(this.messageHandler.window);
    this.#loadListener.on("DOMContentLoaded", this.#onDOMContentLoaded);
    this.#loadListener.on("load", this.#onLoad);
  }

  destroy() {
    this.#loadListener.destroy();
  }

  #startListening() {
    if (!this.#listeningToDOMContentLoaded && !this.#listeningToLoad) {
      this.#loadListener.startListening();
    }
  }

  #subscribeEvent(event) {
    switch (event) {
      case "browsingContext.DOMContentLoaded":
        this.#startListening();
        this.#listeningToDOMContentLoaded = true;
        break;
      case "browsingContext.load":
        this.#startListening();
        this.#listeningToLoad = true;
        break;
    }
  }

  #unsubscribeEvent(event) {
    switch (event) {
      case "browsingContext.DOMContentLoaded":
        this.#listeningToDOMContentLoaded = false;
        break;
      case "browsingContext.load":
        this.#listeningToLoad = false;
        break;
    }

    if (!this.#listeningToDOMContentLoaded && !this.#listeningToLoad) {
      this.#loadListener.stopListening();
    }
  }

  #onDOMContentLoaded = (eventName, data) => {
    if (this.#listeningToDOMContentLoaded) {
      this.messageHandler.emitEvent("browsingContext.DOMContentLoaded", {
        baseURL: data.target.baseURI,
        contextId: this.messageHandler.contextId,
        documentURL: data.target.URL,
        innerWindowId: this.messageHandler.innerWindowId,
        readyState: data.target.readyState,
      });
    }
  };

  #onLoad = (eventName, data) => {
    if (this.#listeningToLoad) {
      this.emitEvent("browsingContext.load", {
        context: this.messageHandler.context,
        // TODO: The navigation id should be a real id mapped to the navigation.
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=1763122
        navigation: null,
        url: data.target.baseURI,
      });
    }
  };

  /**
   * Internal commands
   */

  _applySessionData(params) {
    // TODO: Bug 1775231. Move this logic to a shared module or an abstract
    // class.
    const { category, added = [], removed = [] } = params;
    if (category === "event") {
      for (const event of added) {
        this.#subscribeEvent(event);
      }
      for (const event of removed) {
        this.#unsubscribeEvent(event);
      }
    }
  }

  _getBaseURL() {
    return this.messageHandler.window.document.baseURI;
  }
}

const browsingContext = BrowsingContextModule;
