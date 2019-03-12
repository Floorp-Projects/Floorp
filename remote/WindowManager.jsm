/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "TabManager",
  "TabObserver",
  "WindowObserver",
];

const {DOMContentLoadedPromise} = ChromeUtils.import("chrome://remote/content/Sync.jsm");
const {EventEmitter} = ChromeUtils.import("resource://gre/modules/EventEmitter.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * The WindowManager provides tooling for application-agnostic
 * observation of windows, tabs, and content browsers as they are
 * created and destroyed.
 */

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
      this.onTabOpen(tab);
    }

    window.addEventListener("TabOpen", ({target}) => this.onTabOpen(target));
    window.addEventListener("TabClose", ({target}) => this.onTabClose(target));
  }

  onWindowClose(window) {
    // TODO(ato): Is TabClose fired when the window closes?
  }
}

var TabManager = {
  addTab() {
    const window = Services.wm.getMostRecentWindow("navigator:browser");
    const { gBrowser } = window;
    const tab = gBrowser.addTab("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    gBrowser.selectedTab = tab;
    return tab;
  },
};
