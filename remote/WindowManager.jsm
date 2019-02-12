/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "BrowserObserver",
  "TabObserver",
  "WindowObserver",
  "WindowManager",
];

const {DOMContentLoadedPromise} = ChromeUtils.import("chrome://remote/content/Sync.jsm");
const {EventEmitter} = ChromeUtils.import("chrome://remote/content/EventEmitter.jsm");
const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

/**
 * The WindowManager provides tooling for application-agnostic observation
 * of windows, tabs, and content browsers as they are created and destroyed.
 */

// TODO(ato):
//
// The DOM team is working on pulling browsing context related behaviour,
// such as window and tab handling, out of product code and into the platform.
// This will have implication for the remote agent,
// and as the platform gains support for product-independent events
// we can likely get rid of this entire module.
//
// Seen below, BrowserObserver in particular tries to emulate content
// browser tracking across host process changes.

/**
 * Observes DOMWindows as they open and close.
 *
 * The WindowObserver.Event.Open event fires when a window opens.
 * The WindowObserver.Event.Close event fires when a window closes.
 */
class WindowObserver {
  /**
   * @param {boolean?} [false] registerExisting
   *     Events will be despatched for the ChromeWindows that exist
   *     at the time the observer is started.
   */
  constructor({registerExisting = false} = {}) {
    this.registerExisting = registerExisting;
    EventEmitter.decorate(this);
  }

  async start() {
    if (this.registerExisting) {
      for (const window of Services.wm.getEnumerator("navigator:browser")) {
        await this.onOpenWindow(window);
      }
    }

    Services.wm.addListener(this);
  }

  stop() {
    Services.wm.removeListener(this);
  }

  // nsIWindowMediatorListener

  async onOpenWindow(xulWindow) {
    const window = xulWindow
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindow);
    await new DOMContentLoadedPromise(window);
    this.emit("open", window);
  }

  onCloseWindow(xulWindow) {
    const window = xulWindow
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindow);
    this.emit("close", window);
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIWindowMediatorListener]);
  }
}

/**
 * Observe Firefox tabs as they open and close.
 *
 * "open" fires when a tab opens.
 * "close" fires when a tab closes.
 */
class TabObserver {
  /**
   * @param {boolean?} [false] registerExisting
   *     Events will be fired for ChromeWIndows and their respective tabs
   *     at the time when the observer is started.
   */
  constructor({registerExisting = false} = {}) {
    this.windows = new WindowObserver({registerExisting});
    EventEmitter.decorate(this);
  }

  async start() {
    this.windows.on("open", this.onWindowOpen.bind(this));
    this.windows.on("close", this.onWindowClose.bind(this));
    await this.windows.start();
  }

  stop() {
    this.windows.off("open");
    this.windows.off("close");
    this.windows.stop();
  }

  onTabOpen(tab) {
    this.emit("open", tab);
  }

  onTabClose(tab) {
    this.emit("close", tab);
  }

  // WindowObserver

  async onWindowOpen(window) {
    if (!window.gBrowser) {
      return;
    }

    for (const tab of window.gBrowser.tabs) {
      this.onTabOpen(tab);
    }

    window.addEventListener("TabOpen", ({target}) => this.onTabOpen(target));
    window.addEventListener("TabClose", ({target}) => this.onTabClose(target));
  }

  onWindowClose(window) {
    // TODO(ato): Is TabClose fired when the window closes?
  }
}

/**
 * BrowserObserver is more powerful than TabObserver,
 * as it watches for any content browser appearing anywhere in Gecko.
 * TabObserver on the other hand is limited to browsers associated with a tab.
 *
 * This class is currently not used by the remote agent,
 * but leave it in here because we may have use for it later
 * if we decide to allow Marionette-style chrome automation.
 */
class BrowserObserver {
  constructor() {
    EventEmitter.decorate(this);
  }

  start() {
    // TODO(ato): Theoretically it would be better to use ChromeWindow#getGroupMessageManager("Browsers")
    // TODO(ato): Browser:Init does not cover browsers living in the parent process
    Services.mm.addMessageListener("Browser:Init", this);
    Services.obs.addObserver(this, "message-manager-disconnect");
  }

  stop() {
    Services.mm.removeMessageListener("Browser:Init", this);
    Services.obs.removeObserver(this, "message-manager-disconnect");
  }

  onBrowserInit(browser) {
    this.emit("connected", browser);
  }

  onMessageManagerDisconnect(browser) {
    if (!browser.isConnected) {
      this.emit("disconnected", browser);
    }
  }

  // nsIMessageListener

  receiveMessage({name, target}) {
    switch (name) {
      case "Browser:Init":
        this.onBrowserInit(target);
        break;

      default:
        log.warn("Unknown IPC message form browser: " + name);
        break;
    }
  }

  // nsIObserver

  observe(subject, topic) {
    switch (topic) {
      case "message-manager-disconnect":
        this.onMessageManagerDisconnect(subject);
        break;

      default:
        log.warn("Unknown system observer notification: " + topic);
    }
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([
      Ci.nsIMessageListener,
      Ci.nsIObserver,
    ]);
  }
}

/**
 * Determine if WindowProxy is part of the boundary window.
 *
 * @param {DOMWindow} boundary
 * @param {DOMWindow} target
 *
 * @return {boolean}
 */
function isWindowIncluded(boundary, target) {
  if (target === boundary) {
    return true;
  }

  // TODO(ato): Pretty sure this is not Fission compatible,
  // but then this is a problem that needs to be solved in nsIConsoleAPI.
  const {parent} = target;
  if (!parent || parent === boundary) {
    return false;
  }

  return isWindowIncluded(boundary, parent);
}

var WindowManager = {isWindowIncluded};
