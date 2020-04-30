/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabObserver"];

const { EventPromise } = ChromeUtils.import("chrome://remote/content/Sync.jsm");
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
    EventEmitter.decorate(this);

    this.registerExisting = registerExisting;

    this.onTabOpen = this.onTabOpen.bind(this);
    this.onTabClose = this.onTabClose.bind(this);
  }

  async start() {
    Services.wm.addListener(this);

    if (this.registerExisting) {
      // Start listening for events on already open windows
      for (const win of Services.wm.getEnumerator("navigator:browser")) {
        this._registerDOMWindow(win);
      }
    }
  }

  stop() {
    Services.wm.removeListener(this);

    // Stop listening for events on still opened windows
    for (const win of Services.wm.getEnumerator("navigator:browser")) {
      this._unregisterDOMWindow(win);
    }
  }

  // Event emitters

  onTabOpen({ target }) {
    this.emit("open", target);
  }

  onTabClose({ target }) {
    this.emit("close", target);
  }

  // Internal methods

  _registerDOMWindow(win) {
    for (const tab of win.gBrowser.tabs) {
      // a missing linkedBrowser means the tab is still initialising,
      // and a TabOpen event will fire once it is ready
      if (!tab.linkedBrowser) {
        continue;
      }

      this.onTabOpen({ target: tab });
    }

    win.gBrowser.tabContainer.addEventListener("TabOpen", this.onTabOpen);
    win.gBrowser.tabContainer.addEventListener("TabClose", this.onTabClose);
  }

  _unregisterDOMWindow(win) {
    for (const tab of win.gBrowser.tabs) {
      // a missing linkedBrowser means the tab is still initialising
      if (!tab.linkedBrowser) {
        continue;
      }

      // Emulate custom "TabClose" events because that event is not
      // fired for each of the tabs when the window closes.
      this.onTabClose({ target: tab });
    }

    win.gBrowser.tabContainer.removeEventListener("TabOpen", this.onTabOpen);
    win.gBrowser.tabContainer.removeEventListener("TabClose", this.onTabClose);
  }

  // nsIWindowMediatorListener

  async onOpenWindow(xulWindow) {
    const win = xulWindow.docShell.domWindow;

    await new EventPromise(win, "load");

    // Return early if it's not a browser window
    if (
      win.document.documentElement.getAttribute("windowtype") !=
      "navigator:browser"
    ) {
      return;
    }

    this._registerDOMWindow(win);
  }

  onCloseWindow(xulWindow) {
    const win = xulWindow.docShell.domWindow;

    // Return early if it's not a browser window
    if (
      win.document.documentElement.getAttribute("windowtype") !=
      "navigator:browser"
    ) {
      return;
    }

    this._unregisterDOMWindow(win);
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIWindowMediatorListener]);
  }
}
