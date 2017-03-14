/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewContent"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/GeckoViewModule.jsm");

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "ViewContent");

function debug(aMsg) {
  // dump(aMsg);
}

class GeckoViewContent extends GeckoViewModule {
  init() {
    this.messageManager.loadFrameScript("chrome://browser/content/GeckoViewContent.js", true);
    this.messageManager.addMessageListener("GeckoView:DOMTitleChanged", this);
  }

  handleEvent(aEvent) {
    debug("handleEvent: aEvent.type=" + aEvent.type);
  }

  // Message manager event handler.
  receiveMessage(aMsg) {
    debug("receiveMessage " + aMsg.name);

    switch (aMsg.name) {
      case "GeckoView:DOMTitleChanged":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMTitleChanged",
          title: aMsg.data.title
        });
        break;
    }
  }
}
