/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewContent"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewContent");

var DEBUG = false;

class GeckoViewContent {
  constructor(_window, _browser, _windowEventDispatcher) {
    this.window = _window;
    this.browser = _browser;
    this.windowEventDispatcher = _windowEventDispatcher;

    this.window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = this;

    this.messageManager.loadFrameScript("chrome://browser/content/GeckoViewContent.js", true);
    this.messageManager.addMessageListener("GeckoView:DOMTitleChanged", this);
  }

  get messageManager() {
    return this.browser.messageManager;
  }

  handleEvent(event) {
    if (DEBUG) {
      dump("handleEvent: event.type=" + event.type);
    }
  }

  // Message manager event handler.
  receiveMessage(msg) {
    if (DEBUG) {
      dump("receiveMessage " + msg.name);
    }

    switch (msg.name) {
      case "GeckoView:DOMTitleChanged":
        this.windowEventDispatcher.sendRequest({ type: "GeckoView:DOMTitleChanged", title: msg.data.title });
        break;
    }
  }
}
