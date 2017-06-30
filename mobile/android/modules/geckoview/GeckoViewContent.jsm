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
    this.messageManager.loadFrameScript(
      "chrome://geckoview/content/GeckoViewContent.js", true);
  }

  register() {
    this.window.addEventListener("MozDOMFullScreen:Entered", this,
                                 /* capture */ true, /* untrusted */ false);
    this.window.addEventListener("MozDOMFullScreen:Exited", this,
                                 /* capture */ true, /* untrusted */ false);

    this.eventDispatcher.registerListener(this, "GeckoViewContent:ExitFullScreen");
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenRequest", this);
    this.messageManager.addMessageListener("GeckoView:DOMTitleChanged", this);
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug("onEvent: " + aEvent);
    switch (aEvent) {
      case "GeckoViewContent:ExitFullScreen":
        this.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenExited");
        break;
    }
  }

  unregister() {
    this.window.removeEventListener("MozDOMFullScreen:Entered", this,
                                    /* capture */ true);
    this.window.removeEventListener("MozDOMFullScreen:Exited", this,
                                    /* capture */ true);
    this.eventDispatcher.unregisterListener(this, "GeckoViewContent:ExitFullScreen");
    this.messageManager.removeMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.removeMessageListener("GeckoView:DOMFullscreenRequest", this);
    this.messageManager.removeMessageListener("GeckoView:DOMTitleChanged", this);
  }

  // DOM event handler
  handleEvent(aEvent) {
    debug("handleEvent: aEvent.type=" + aEvent.type);

    switch (aEvent.type) {
      case "MozDOMFullscreen:Entered":
        if (this.browser == aEvent.target) {
          // Remote browser; dispatch to content process.
          this.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenEntered");
        }
        break;
      case "MozDOMFullscreen:Exited":
        this.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenExited");
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
