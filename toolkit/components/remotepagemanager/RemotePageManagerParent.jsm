/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RemotePages", "RemotePageManager"];

/*
 * Using the RemotePageManager:
 * * Create a new page listener by calling 'new RemotePages(URI)' which
 *   then injects functions like RPMGetBoolPref() into the registered page.
 *   One can then use those exported functions to communicate between
 *   child and parent.
 *
 * * When adding a new consumer of RPM that relies on other functionality
 *   then simple message passing provided by the RPM, then one has to
 *   whitelist permissions for the new URI within the RPMAccessManager
 *   from MessagePort.jsm.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { MessageListener, MessagePort } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/MessagePort.jsm"
);

/**
 * Creates a RemotePages object which listens for new remote pages of some
 * particular URLs. A "RemotePage:Init" message will be dispatched to this
 * object for every page loaded. Message listeners added to this object receive
 * messages from all loaded pages from the requested urls.
 */
class RemotePages {
  constructor(urls) {
    this.urls = Array.isArray(urls) ? urls : [urls];
    this.messagePorts = new Set();
    this.listener = new MessageListener();
    this.destroyed = false;

    this.portCreated = this.portCreated.bind(this);
    this.portMessageReceived = this.portMessageReceived.bind(this);

    for (const url of this.urls) {
      RemotePageManager.addRemotePageListener(url, this.portCreated);
    }
  }

  destroy() {
    for (const url of this.urls) {
      RemotePageManager.removeRemotePageListener(url);
    }

    for (let port of this.messagePorts.values()) {
      this.removeMessagePort(port);
    }

    this.messagePorts = null;
    this.listener = null;
    this.destroyed = true;
  }

  // Called when a page matching one of the urls has loaded in a frame.
  portCreated(port) {
    this.messagePorts.add(port);

    port.loaded = false;
    port.addMessageListener("RemotePage:Load", this.portMessageReceived);
    port.addMessageListener("RemotePage:Unload", this.portMessageReceived);

    for (let name of this.listener.keys()) {
      this.registerPortListener(port, name);
    }

    this.listener.callListeners({ target: port, name: "RemotePage:Init" });
  }

  // A message has been received from one of the pages
  portMessageReceived(message) {
    switch (message.name) {
      case "RemotePage:Load":
        message.target.loaded = true;
        break;
      case "RemotePage:Unload":
        message.target.loaded = false;
        this.removeMessagePort(message.target);
        break;
    }

    this.listener.callListeners(message);
  }

  // A page has closed
  removeMessagePort(port) {
    for (let name of this.listener.keys()) {
      port.removeMessageListener(name, this.portMessageReceived);
    }

    port.removeMessageListener("RemotePage:Load", this.portMessageReceived);
    port.removeMessageListener("RemotePage:Unload", this.portMessageReceived);
    this.messagePorts.delete(port);
  }

  registerPortListener(port, name) {
    port.addMessageListener(name, this.portMessageReceived);
  }

  // Sends a message to all known pages
  sendAsyncMessage(name, data = null) {
    for (let port of this.messagePorts.values()) {
      try {
        port.sendAsyncMessage(name, data);
      } catch (e) {
        // Unless the port is in the process of unloading, something strange
        // happened but allow other ports to receive the message
        if (e.result !== Cr.NS_ERROR_NOT_INITIALIZED) {
          Cu.reportError(e);
        }
      }
    }
  }

  addMessageListener(name, callback) {
    if (this.destroyed) {
      throw new Error("RemotePages has been destroyed");
    }

    if (!this.listener.has(name)) {
      for (let port of this.messagePorts.values()) {
        this.registerPortListener(port, name);
      }
    }

    this.listener.addMessageListener(name, callback);
  }

  removeMessageListener(name, callback) {
    if (this.destroyed) {
      throw new Error("RemotePages has been destroyed");
    }

    this.listener.removeMessageListener(name, callback);
  }

  portsForBrowser(browser) {
    return [...this.messagePorts].filter(port => port.browser == browser);
  }
}

// Only exposes the public properties of the MessagePort
function publicMessagePort(port) {
  let properties = [
    "addMessageListener",
    "removeMessageListener",
    "sendAsyncMessage",
    "destroy",
  ];

  let clean = {};
  for (let property of properties) {
    clean[property] = port[property].bind(port);
  }

  Object.defineProperty(clean, "portID", {
    enumerable: true,
    get() {
      return port.portID;
    },
  });

  if (port instanceof ChromeMessagePort) {
    Object.defineProperty(clean, "browser", {
      enumerable: true,
      get() {
        return port.browser;
      },
    });

    Object.defineProperty(clean, "url", {
      enumerable: true,
      get() {
        return port.url;
      },
    });
  }

  return clean;
}

// The chome side of a message port
class ChromeMessagePort extends MessagePort {
  constructor(browser, portID, url) {
    super(browser.messageManager, portID);

    this._browser = browser;
    this._permanentKey = browser.permanentKey;
    this._url = url;

    Services.obs.addObserver(this, "message-manager-disconnect");
    this.publicPort = publicMessagePort(this);

    this.swapBrowsers = this.swapBrowsers.bind(this);
    this._browser.addEventListener("SwapDocShells", this.swapBrowsers);
  }

  get browser() {
    return this._browser;
  }

  get url() {
    return this._url;
  }

  // Called when the docshell is being swapped with another browser. We have to
  // update to use the new browser's message manager
  swapBrowsers({ detail: newBrowser }) {
    // We can see this event for the new browser before the swap completes so
    // check that the browser we're tracking has our permanentKey.
    if (this._browser.permanentKey != this._permanentKey) {
      return;
    }

    this._browser.removeEventListener("SwapDocShells", this.swapBrowsers);

    this._browser = newBrowser;
    this.swapMessageManager(newBrowser.messageManager);

    this._browser.addEventListener("SwapDocShells", this.swapBrowsers);
  }

  // Called when a message manager has been disconnected indicating that the
  // tab has closed or crashed
  observe(messageManager) {
    if (messageManager != this.messageManager) {
      return;
    }

    this.listener.callListeners({
      target: this.publicPort,
      name: "RemotePage:Unload",
      data: null,
    });
    this.destroy();
  }

  // Called when the content process is requesting some data.
  async handleRequest(name, data) {
    throw new Error(`Unknown request ${name}.`);
  }

  // Called when a message is received from the message manager.
  handleMessage(messagedata) {
    let message = {
      target: this.publicPort,
      name: messagedata.name,
      data: messagedata.data,
      browsingContextID: messagedata.browsingContextID,
    };
    this.listener.callListeners(message);

    if (messagedata.name == "RemotePage:Unload") {
      this.destroy();
    }
  }

  destroy() {
    try {
      this._browser.removeEventListener("SwapDocShells", this.swapBrowsers);
    } catch (e) {
      // It's possible the browser instance is already dead so we can just ignore
      // this error.
    }

    this._browser = null;
    Services.obs.removeObserver(this, "message-manager-disconnect");
    super.destroy.call(this);
  }
}

// Allows callers to register to connect to specific content pages. Registration
// is done through the addRemotePageListener method
var RemotePageManagerInternal = {
  // The currently registered remote pages
  pages: new Map(),

  // Initialises all the needed listeners
  init() {
    Services.mm.addMessageListener(
      "RemotePage:InitPort",
      this.initPort.bind(this)
    );
    this.updateProcessUrls();
  },

  updateProcessUrls() {
    Services.ppmm.sharedData.set(
      "RemotePageManager:urls",
      new Set(this.pages.keys())
    );
    Services.ppmm.sharedData.flush();
  },

  // Registers interest in a remote page. A callback is called with a port for
  // the new page when loading begins (i.e. the page hasn't actually loaded yet).
  // Only one callback can be registered per URL.
  addRemotePageListener(url, callback) {
    if (this.pages.has(url)) {
      throw new Error("Remote page already registered: " + url);
    }

    this.pages.set(url, callback);
    this.updateProcessUrls();
  },

  // Removes any interest in a remote page.
  removeRemotePageListener(url) {
    if (!this.pages.has(url)) {
      throw new Error("Remote page is not registered: " + url);
    }

    this.pages.delete(url);
    this.updateProcessUrls();
  },

  // A remote page has been created and a port is ready in the content side
  initPort({ target: browser, data: { url, portID } }) {
    let callback = this.pages.get(url);
    if (!callback) {
      Cu.reportError("Unexpected remote page load: " + url);
      return;
    }

    let port = new ChromeMessagePort(browser, portID, url);
    callback(port.publicPort);
  },
};

if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
  throw new Error("RemotePageManager can only be used in the main process.");
}

RemotePageManagerInternal.init();

// The public API for the above object
var RemotePageManager = {
  addRemotePageListener: RemotePageManagerInternal.addRemotePageListener.bind(
    RemotePageManagerInternal
  ),
  removeRemotePageListener: RemotePageManagerInternal.removeRemotePageListener.bind(
    RemotePageManagerInternal
  ),
};
