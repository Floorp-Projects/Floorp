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
  truncate: "chrome://remote/content/shared/Format.jsm",
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
  // Start the listener right away to avoid race conditions.
  const listener = new ProgressListener(browsingContext.webProgress);
  const navigated = listener.start();

  // Right after a browsing context has been attached it could happen that
  // no window global has been set yet. Consider this as nothing has been
  // loaded yet.
  let isInitial = true;
  if (browsingContext.currentWindowGlobal) {
    isInitial = browsingContext.currentWindowGlobal.isInitialDocument;
  }

  // If the current document is not the initial "about:blank" and is also
  // no longer loading, assume the navigation is done and return.
  if (!isInitial && !listener.isLoadingDocument) {
    logger.trace(
      truncate`[${browsingContext.id}] Document already finished loading: ${browsingContext.currentURI?.spec}`
    );

    listener.stop();
    return Promise.resolve();
  }

  return navigated;
}

/**
 * WebProgressListener to observe page loads.
 *
 * @param {WebProgress} webProgress
 *     The web progress to attach the listener to.
 * @param {Object=} options
 * @param {Number=} options.unloadTimeout
 *     Time to allow before the page gets unloaded. Defaults to 200ms.
 */
class ProgressListener {
  #resolve;
  #unloadTimeout;
  #unloadTimer;
  #webProgress;

  constructor(webProgress, options = {}) {
    const { unloadTimeout = 200 } = options;

    this.#resolve = null;
    this.#unloadTimeout = unloadTimeout;
    this.#unloadTimer = null;
    this.#webProgress = webProgress;
  }

  get browsingContext() {
    return this.#webProgress.browsingContext;
  }

  get currentURI() {
    return this.#webProgress.browsingContext.currentURI;
  }

  get isLoadingDocument() {
    return this.#webProgress.isLoadingDocument;
  }

  onStateChange(progress, request, flag, status) {
    const isStart = flag & Ci.nsIWebProgressListener.STATE_START;
    const isStop = flag & Ci.nsIWebProgressListener.STATE_STOP;

    if (isStart) {
      this.#unloadTimer?.cancel();
      logger.trace(
        truncate`[${this.browsingContext.id}] Web progress state=start: ${this.currentURI?.spec}`
      );
    }

    if (isStop) {
      logger.trace(
        truncate`this.browsingContext.id}] Web progress state=stop: ${this.currentURI?.spec}`
      );

      this.#resolve();
      this.stop();
    }
  }

  /**
   * Start observing web progress changes.
   *
   * @returns {Promise}
   *     A promise that will resolve when the navigation has been finished.
   */
  start() {
    if (this.#resolve) {
      throw new Error(`Progress listener already started`);
    }

    const promise = new Promise(resolve => (this.#resolve = resolve));

    this.#webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );

    webProgressListeners.add(this);

    if (!this.#webProgress.isLoadingDocument) {
      // If the document is not loading yet wait some time for the navigation
      // to be started.
      this.#unloadTimer = Cc["@mozilla.org/timer;1"].createInstance(
        Ci.nsITimer
      );
      this.#unloadTimer.initWithCallback(
        () => {
          logger.trace(
            truncate`[${this.browsingContext.id}] No navigation detected: ${this.currentURI?.spec}`
          );
          this.#resolve();
          this.stop();
        },
        this.#unloadTimeout,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
    }

    return promise;
  }

  /**
   * Stop observing web progress changes.
   */
  stop() {
    if (!this.#resolve) {
      throw new Error(`Progress listener not yet started`);
    }

    this.#unloadTimer?.cancel();
    this.#unloadTimer = null;

    this.#resolve = null;

    this.#webProgress.removeProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );
    webProgressListeners.delete(this);
  }

  get toString() {
    return `[object ${this.constructor.name}]`;
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([
      "nsIWebProgressListener",
      "nsISupportsWeakReference",
    ]);
  }
}
