/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ChildMessagePort"];

const { MessagePort } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/MessagePort.jsm"
);

// The content side of a message port
class ChildMessagePort extends MessagePort {
  constructor(window) {
    let portID =
      Services.appinfo.processID + ":" + ChildMessagePort.nextPortID++;
    super(window.docShell.messageManager, portID);

    this.window = window;

    // Add functionality to the content page
    Cu.exportFunction(this.sendAsyncMessage.bind(this), window, {
      defineAs: "RPMSendAsyncMessage",
    });
    Cu.exportFunction(this.addMessageListener.bind(this), window, {
      defineAs: "RPMAddMessageListener",
      allowCallbacks: true,
    });
    Cu.exportFunction(this.removeMessageListener.bind(this), window, {
      defineAs: "RPMRemoveMessageListener",
      allowCallbacks: true,
    });

    // The actor form only needs the functions set up above. The actor
    // will send and receive messages directly.
    if (!(this.messageManager instanceof Ci.nsIMessageSender)) {
      return;
    }

    // Send a message for load events
    let loadListener = () => {
      this.sendAsyncMessage("RemotePage:Load");
      window.removeEventListener("load", loadListener);
    };
    window.addEventListener("load", loadListener);

    // Destroy the port when the window is unloaded
    window.addEventListener("unload", () => {
      try {
        this.sendAsyncMessage("RemotePage:Unload");
      } catch (e) {
        // If the tab has been closed the frame message manager has already been
        // destroyed
      }
      this.destroy();
    });

    // Tell the main process to set up its side of the message pipe.
    this.messageManager.sendAsyncMessage("RemotePage:InitPort", {
      portID,
      url: window.document.documentURI.replace(/[\#|\?].*$/, ""),
    });
  }

  // Called when the content process is requesting some data.
  async handleRequest(name, data) {
    throw new Error(`Unknown request ${name}.`);
  }

  // Called when a message is received from the message manager or actor.
  handleMessage(messagedata) {
    let message = {
      name: messagedata.name,
      data: messagedata.data,
    };
    this.listener.callListeners(Cu.cloneInto(message, this.window));
  }

  destroy() {
    this.window = null;
    super.destroy.call(this);
  }
}

ChildMessagePort.nextPortID = 0;
