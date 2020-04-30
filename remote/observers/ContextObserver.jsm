/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helper class to coordinate Runtime and Page events.
 * Events have to be sent in the following order:
 *  - Runtime.executionContextDestroyed
 *  - Page.frameNavigated
 *  - Runtime.executionContextCreated
 *
 * This class also handles the special case of Pages going from/to the BF cache.
 * When you navigate to a new URL, the previous document may be stored in the BF Cache.
 * All its asynchronous operations are frozen (XHR, timeouts, ...) and a `pagehide` event
 * is fired for this document. We then navigate to the new URL.
 * If the user navigates back to the previous page, the page is resurected from the
 * cache. A `pageshow` event is fired and its asynchronous operations are resumed.
 *
 * When a page is in the BF Cache, we should consider it as frozen and shouldn't try
 * to execute any javascript. So that the ExecutionContext should be considered as
 * being destroyed and the document navigated.
 */

var EXPORTED_SYMBOLS = ["ContextObserver"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { executeSoon } = ChromeUtils.import("chrome://remote/content/Sync.jsm");

class ContextObserver {
  constructor(chromeEventHandler) {
    this.chromeEventHandler = chromeEventHandler;
    EventEmitter.decorate(this);

    this.chromeEventHandler.addEventListener("DOMWindowCreated", this, {
      mozSystemGroup: true,
    });

    // Listen for pageshow and pagehide to track pages going in/out to/from the BF Cache
    this.chromeEventHandler.addEventListener("pageshow", this, {
      mozSystemGroup: true,
    });
    this.chromeEventHandler.addEventListener("pagehide", this, {
      mozSystemGroup: true,
    });

    Services.obs.addObserver(this, "inner-window-destroyed");
  }

  destructor() {
    this.chromeEventHandler.removeEventListener("DOMWindowCreated", this, {
      mozSystemGroup: true,
    });
    this.chromeEventHandler.removeEventListener("pageshow", this, {
      mozSystemGroup: true,
    });
    this.chromeEventHandler.removeEventListener("pagehide", this, {
      mozSystemGroup: true,
    });
    Services.obs.removeObserver(this, "inner-window-destroyed");
  }

  handleEvent({ type, target, persisted }) {
    const window = target.defaultView;
    if (window != this.chromeEventHandler.ownerGlobal) {
      // Ignore iframes for now.
      return;
    }
    const { windowUtils } = window;
    const frameId = window.docShell.browsingContext.id.toString();
    const id = windowUtils.currentInnerWindowID;
    switch (type) {
      case "DOMWindowCreated":
        // Do not pass `id` here as that's the new document ID instead of the old one
        // that is destroyed. Instead, pass the frameId and let the listener figure out
        // what ExecutionContext(s) to destroy.
        this.emit("context-destroyed", { frameId });
        this.emit("frame-navigated", { frameId, window });
        this.emit("context-created", { windowId: id, window });
        // Delay script-loaded to allow context cleanup to happen first
        executeSoon(() => {
          this.emit("script-loaded");
        });
        break;
      case "pageshow":
        // `persisted` is true when this is about a page being resurected from BF Cache
        if (!persisted) {
          return;
        }
        // XXX(ochameau) we might have to emit FrameNavigate here to properly handle BF Cache
        // scenario in Page domain events
        this.emit("context-created", { windowId: id, window });
        this.emit("script-loaded");
        break;

      case "pagehide":
        // `persisted` is true when this is about a page being frozen into BF Cache
        if (!persisted) {
          return;
        }
        this.emit("context-destroyed", { windowId: id });
        break;
    }
  }

  // "inner-window-destroyed" observer service listener
  observe(subject, topic, data) {
    const innerWindowID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    this.emit("context-destroyed", { windowId: innerWindowID });
  }
}
