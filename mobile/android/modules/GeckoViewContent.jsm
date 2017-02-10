/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewContent"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/GeckoViewModule.jsm");

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewContent");

var DEBUG = false;

class GeckoViewContent extends GeckoViewModule {
  init() {
    this.window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = this;

    this.messageManager.loadFrameScript("chrome://browser/content/GeckoViewContent.js", true);
    this.messageManager.addMessageListener("GeckoView:DOMTitleChanged", this);
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
        this.eventDispatcher.sendRequest({ type: "GeckoView:DOMTitleChanged", title: msg.data.title });
        break;
    }
  }
}
