/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContent"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                       {}).AndroidLog.d.bind(null, "ViewContent"));

function debug(aMsg) {
  // dump(aMsg);
}

class GeckoViewContent extends GeckoViewModule {
  onInit() {
    this.eventDispatcher.registerListener(this, [
      "GeckoView:SetActive"
    ]);
  }

  onEnable() {
    this.registerContent("chrome://geckoview/content/GeckoViewContent.js");

    this.window.addEventListener("MozDOMFullScreen:Entered", this,
                                 /* capture */ true, /* untrusted */ false);
    this.window.addEventListener("MozDOMFullScreen:Exited", this,
                                 /* capture */ true, /* untrusted */ false);

    this.registerListener([
        "GeckoViewContent:ExitFullScreen",
        "GeckoView:ZoomToInput",
    ]);

    this.messageManager.addMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenRequest", this);
  }

  onDisable() {
    this.window.removeEventListener("MozDOMFullScreen:Entered", this,
                                    /* capture */ true);
    this.window.removeEventListener("MozDOMFullScreen:Exited", this,
                                    /* capture */ true);

    this.unregisterListener();

    this.messageManager.removeMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.removeMessageListener("GeckoView:DOMFullscreenRequest", this);
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug("onEvent: " + aEvent);
    switch (aEvent) {
      case "GeckoViewContent:ExitFullScreen":
        this.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenExited");
        break;
      case "GeckoView:ZoomToInput":
        this.messageManager.sendAsyncMessage(aEvent);
        break;
      case "GeckoView:SetActive":
        if (aData.active) {
          this.browser.setAttribute("primary", "true");
          this.browser.focus();
          this.browser.docShellIsActive = true;
        } else {
          this.browser.removeAttribute("primary");
          this.browser.docShellIsActive = false;
          this.browser.blur();
        }
        break;
    }
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
    }
  }
}
