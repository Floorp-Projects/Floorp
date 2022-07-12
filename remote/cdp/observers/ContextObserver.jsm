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

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",

  executeSoon: "chrome://remote/content/shared/Sync.jsm",
});

class ContextObserver {
  constructor(chromeEventHandler) {
    this.chromeEventHandler = chromeEventHandler;
    lazy.EventEmitter.decorate(this);

    this._fissionEnabled = Services.appinfo.fissionAutostart;

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

    Services.obs.addObserver(this, "document-element-inserted");
    Services.obs.addObserver(this, "inner-window-destroyed");

    // With Fission disabled the `DOMWindowCreated` event is fired too late.
    // Use the `webnavigation-create` notification instead.
    if (!this._fissionEnabled) {
      Services.obs.addObserver(this, "webnavigation-create");
    }
    Services.obs.addObserver(this, "webnavigation-destroy");
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

    Services.obs.removeObserver(this, "document-element-inserted");
    Services.obs.removeObserver(this, "inner-window-destroyed");

    if (!this._fissionEnabled) {
      Services.obs.removeObserver(this, "webnavigation-create");
    }
    Services.obs.removeObserver(this, "webnavigation-destroy");
  }

  handleEvent({ type, target, persisted }) {
    const window = target.defaultView;
    const frameId = window.browsingContext.id;
    const id = window.windowGlobalChild.innerWindowId;

    switch (type) {
      case "DOMWindowCreated":
        // Do not pass `id` here as that's the new document ID instead of the old one
        // that is destroyed. Instead, pass the frameId and let the listener figure out
        // what ExecutionContext(s) to destroy.
        this.emit("context-destroyed", { frameId });

        // With Fission enabled the frame is attached early enough so that
        // expected network requests and responses are handles afterward.
        // Otherwise send the event when `webnavigation-create` is received.
        if (this._fissionEnabled) {
          this.emit("frame-attached", { frameId, window });
        }

        break;

      case "pageshow":
        // `persisted` is true when this is about a page being resurected from BF Cache
        if (!persisted) {
          return;
        }
        // XXX(ochameau) we might have to emit FrameNavigate here to properly handle BF Cache
        // scenario in Page domain events
        this.emit("context-created", { windowId: id, window });
        this.emit("script-loaded", { windowId: id, window });
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

  observe(subject, topic, data) {
    switch (topic) {
      case "document-element-inserted":
        const window = subject.defaultView;

        // Ignore events without a window and those from other tabs
        if (
          !window ||
          window.docShell.chromeEventHandler !== this.chromeEventHandler
        ) {
          return;
        }

        // Send when the document gets attached to the window, and its location
        // is available.
        this.emit("frame-navigated", {
          frameId: window.browsingContext.id,
          window,
        });

        const id = window.windowGlobalChild.innerWindowId;
        this.emit("context-created", { windowId: id, window });
        // Delay script-loaded to allow context cleanup to happen first
        lazy.executeSoon(() => {
          this.emit("script-loaded", { windowId: id, window });
        });
        break;
      case "inner-window-destroyed":
        const windowId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
        this.emit("context-destroyed", { windowId });
        break;
      case "webnavigation-create":
        subject.QueryInterface(Ci.nsIDocShell);
        this.onDocShellCreated(subject);
        break;
      case "webnavigation-destroy":
        subject.QueryInterface(Ci.nsIDocShell);
        this.onDocShellDestroyed(subject);
        break;
    }
  }

  onDocShellCreated(docShell) {
    this.emit("frame-attached", {
      frameId: docShell.browsingContext.id,
      window: docShell.browsingContext.window,
    });
  }

  onDocShellDestroyed(docShell) {
    this.emit("frame-detached", {
      frameId: docShell.browsingContext.id,
    });
  }
}
