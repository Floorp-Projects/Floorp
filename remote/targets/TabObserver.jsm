/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabObserver"];

const { DOMContentLoadedPromise } = ChromeUtils.import(
  "chrome://remote/content/Sync.jsm"
);
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// TODO(ato):
//
// The DOM team is working on pulling browsing context related behaviour,
// such as window and tab handling, out of product code and into the platform.
// This will have implication for the remote agent,
// and as the platform gains support for product-independent events
// we can likely get rid of this entire module.

/**
 * Observes DOMWindows as they open and close.
 *
 * "open" fires when a window opens.
 * "close" fires when a window closes.
 */
class WindowObserver {
  /**
   * @param {boolean?} [false] registerExisting
   *     Events will be despatched for the ChromeWindows that exist
   *     at the time the observer is started.
   */
  constructor({ registerExisting = false } = {}) {
    this.registerExisting = registerExisting;
    EventEmitter.decorate(this);
  }

  async start() {
    if (this.registerExisting) {
      for (const window of Services.wm.getEnumerator("navigator:browser")) {
        await this.onOpenDOMWindow(window);
      }
    }

    Services.wm.addListener(this);
  }

  stop() {
    Services.wm.removeListener(this);
  }

  async onOpenDOMWindow(window) {
    await new DOMContentLoadedPromise(window);
    this.emit("open", window);
  }

  // nsIWindowMediatorListener

  async onOpenWindow(xulWindow) {
    const window = xulWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow);
    await this.onOpenDOMWindow(window);
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
  constructor({ registerExisting = false } = {}) {
    this.windows = new WindowObserver({ registerExisting });
    EventEmitter.decorate(this);
    this.onWindowOpen = this.onWindowOpen.bind(this);
    this.onWindowClose = this.onWindowClose.bind(this);
    this.onTabOpen = this.onTabOpen.bind(this);
    this.onTabClose = this.onTabClose.bind(this);
  }

  async start() {
    this.windows.on("open", this.onWindowOpen);
    this.windows.on("close", this.onWindowClose);
    await this.windows.start();
  }

  stop() {
    this.windows.off("open", this.onWindowOpen);
    this.windows.off("close", this.onWindowClose);
    this.windows.stop();

    // Stop listening for events on still opened windows
    for (const window of Services.wm.getEnumerator("navigator:browser")) {
      this.onWindowClose(window);
    }
  }

  onTabOpen({ target }) {
    this.emit("open", target);
  }

  onTabClose({ target }) {
    this.emit("close", target);
  }

  // WindowObserver

  async onWindowOpen(eventName, window) {
    if (!window.gBrowser) {
      return;
    }

    for (const tab of window.gBrowser.tabs) {
      // a missing linkedBrowser means the tab is still initialising,
      // and a TabOpen event will fire once it is ready
      if (!tab.linkedBrowser) {
        continue;
      }
      this.onTabOpen({ target: tab });
    }

    window.addEventListener("TabOpen", this.onTabOpen);
    window.addEventListener("TabClose", this.onTabClose);
  }

  onWindowClose(window) {
    // TODO(ato): Is TabClose fired when the window closes?

    window.removeEventListener("TabOpen", this.onTabOpen);
    window.removeEventListener("TabClose", this.onTabClose);
  }
}
