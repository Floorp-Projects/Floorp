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
    this.messageManager.loadFrameScript("chrome://geckoview/content/GeckoViewContent.js", true);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenRequest", this);
    this.messageManager.addMessageListener("GeckoView:DOMTitleChanged", this);

    this.window.addEventListener("MozDOMFullscreen:Entered", this,
                                 /* capture */ true, /* untrusted */ false);
    this.window.addEventListener("MozDOMFullscreen:Exited", this,
                                 /* capture */ true, /* untrusted */ false);
  }

  handleEvent(aEvent) {
    debug("handleEvent: aEvent.type=" + aEvent.type);

    switch (aEvent.type) {
      case "MozDOMFullscreen:Entered":
        if (this.browser == aEvent.target) {
          // Remote browser; dispatch to content process.
          this.browser.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenEntered");
        }
        break;
      case "MozDOMFullscreen:Exited":
        this.browser.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenExited");
        break;
    }
  }

  // Message manager event handler.
  receiveMessage(aMsg) {
    debug("receiveMessage " + aMsg.name);

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenExit":
        this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .remoteFrameFullscreenReverted();
        break;
      case "GeckoView:DOMFullscreenRequest":
        this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .remoteFrameFullscreenChanged(aMsg.target);
        break;
      case "GeckoView:DOMTitleChanged":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMTitleChanged",
          title: aMsg.data.title
        });
        break;
    }
  }
}
