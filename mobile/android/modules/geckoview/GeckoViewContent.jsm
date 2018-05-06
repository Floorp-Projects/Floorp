/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContent"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

class GeckoViewContent extends GeckoViewModule {
  onInit() {
    this.registerListener([
        "GeckoViewContent:ExitFullScreen",
        "GeckoView:RestoreState",
        "GeckoView:SaveState",
        "GeckoView:SetActive",
        "GeckoView:ZoomToInput",
    ]);

    this.messageManager.addMessageListener("GeckoView:SaveStateFinish", this);

    this.registerContent("chrome://geckoview/content/GeckoViewContent.js");
  }

  onEnable() {
    this.window.addEventListener("MozDOMFullScreen:Entered", this,
                                 /* capture */ true, /* untrusted */ false);
    this.window.addEventListener("MozDOMFullScreen:Exited", this,
                                 /* capture */ true, /* untrusted */ false);

    this.messageManager.addMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenRequest", this);
  }

  onDisable() {
    this.window.removeEventListener("MozDOMFullScreen:Entered", this,
                                    /* capture */ true);
    this.window.removeEventListener("MozDOMFullScreen:Exited", this,
                                    /* capture */ true);

    this.messageManager.removeMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.removeMessageListener("GeckoView:DOMFullscreenRequest", this);
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug `onEvent: event=${aEvent}, data=${aData}`;

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
      case "GeckoView:SaveState":
        if (!this._saveStateCallbacks) {
          this._saveStateCallbacks = new Map();
          this._saveStateNextId = 0;
        }
        this._saveStateCallbacks.set(this._saveStateNextId, aCallback);
        this.messageManager.sendAsyncMessage("GeckoView:SaveState", {id: this._saveStateNextId});
        this._saveStateNextId++;
        break;
      case "GeckoView:RestoreState":
        this.messageManager.sendAsyncMessage("GeckoView:RestoreState", {state: aData.state});
        break;
    }
  }

  // DOM event handler
  handleEvent(aEvent) {
    debug `handleEvent: ${aEvent.type}`;

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
    debug `receiveMessage: ${aMsg.name}`;

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
      case "GeckoView:SaveStateFinish":
        if (!this._saveStateCallbacks || !this._saveStateCallbacks.has(aMsg.data.id)) {
          warn `Failed to save state due to missing callback`;
          return;
        }
        this._saveStateCallbacks.get(aMsg.data.id).onSuccess(aMsg.data.state);
        this._saveStateCallbacks.delete(aMsg.data.id);
        break;
    }
  }
}
