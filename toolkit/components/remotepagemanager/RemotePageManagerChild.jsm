/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ChildMessagePort"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/remotepagemanager/MessagePort.jsm");

// The content side of a message port
function ChildMessagePort(contentFrame, window) {
  let portID = Services.appinfo.processID + ":" + ChildMessagePort.prototype.nextPortID++;
  MessagePort.call(this, contentFrame, portID);

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
  Cu.exportFunction(this.getBoolPref.bind(this), window, {
    defineAs: "RPMGetBoolPref",
  });
  Cu.exportFunction(this.setBoolPref.bind(this), window, {
    defineAs: "RPMSetBoolPref",
  });
  Cu.exportFunction(this.getFormatURLPref.bind(this), window, {
    defineAs: "RPMGetFormatURLPref",
  });
  Cu.exportFunction(this.isWindowPrivate.bind(this), window, {
    defineAs: "RPMIsWindowPrivate",
  });

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

ChildMessagePort.prototype = Object.create(MessagePort.prototype);

ChildMessagePort.prototype.nextPortID = 0;

// Called when a message is received from the message manager. This could
// have come from any port in the message manager so verify the port ID.
ChildMessagePort.prototype.message = function({ data: messagedata }) {
  if (this.destroyed || (messagedata.portID != this.portID)) {
    return;
  }

  let message = {
    name: messagedata.name,
    data: messagedata.data,
  };
  this.listener.callListeners(Cu.cloneInto(message, this.window));
};

ChildMessagePort.prototype.destroy = function() {
  this.window = null;
  MessagePort.prototype.destroy.call(this);
};


// Listen for pages in any process we're loaded in
var registeredURLs = new Set(Services.cpmm.initialProcessData["RemotePageManager:urls"]);

var observer = (window) => {
  // Strip the hash from the URL, because it's not part of the origin.
  let url = window.document.documentURI.replace(/[\#|\?].*$/, "");

  if (!registeredURLs.has(url))
    return;

  // Get the frame message manager for this window so we can associate this
  // page with a browser element
  let messageManager = window.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDocShell)
                             .QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIContentFrameMessageManager);
  // Set up the child side of the message port
  new ChildMessagePort(messageManager, window);
};
Services.obs.addObserver(observer, "chrome-document-global-created");
Services.obs.addObserver(observer, "content-document-global-created");

// A message from chrome telling us what pages to listen for
Services.cpmm.addMessageListener("RemotePage:Register", ({ data }) => {
  for (let url of data.urls)
    registeredURLs.add(url);
});

// A message from chrome telling us what pages to stop listening for
Services.cpmm.addMessageListener("RemotePage:Unregister", ({ data }) => {
  for (let url of data.urls)
    registeredURLs.delete(url);
});
