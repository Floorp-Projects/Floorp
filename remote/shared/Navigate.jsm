/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["waitForInitialNavigationCompleted"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.get(Log.TYPES.REMOTE_AGENT)
);

// Used to keep weak references of webProgressListeners alive.
const webProgressListeners = new Set();

/**
 * Wait until the initial load of the given browsing context is done.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context to check.
 */
function waitForInitialNavigationCompleted(browsingContext) {
  let listener;

  return new Promise(resolve => {
    listener = new ProgressListener(resolve);
    // Keep a reference to the weak listener so it's not getting gc'ed.
    webProgressListeners.add(listener);

    // Monitor the webprogress listener before checking isLoadingDocument to
    // avoid race conditions.
    browsingContext.webProgress.addProgressListener(
      listener,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );

    // Right after its creation a browsing context doesn't have a window global.
    const isInitial = !browsingContext.currentWindowGlobal;

    if (!browsingContext.webProgress.isLoadingDocument && !isInitial) {
      logger.trace("Initial navigation already completed");
      resolve();
    }
  }).finally(() => {
    browsingContext.webProgress.removeProgressListener(
      listener,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );
    webProgressListeners.delete(listener);
  });
}

class ProgressListener {
  constructor(resolve) {
    this.resolve = resolve;
  }

  onStateChange(progress, request, flag, status) {
    const isStop = flag & Ci.nsIWebProgressListener.STATE_STOP;
    if (isStop) {
      this.resolve();
    }
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([
      "nsIWebProgressListener",
      "nsISupportsWeakReference",
    ]);
  }
}
