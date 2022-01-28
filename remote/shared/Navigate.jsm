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
 * Wait until the initial load of the given WebProgress is done.
 *
 * @param {WebProgress} webProgress
 *     The WebProgress instance to observe.
 * @param {Object=} options
 * @param {Boolean=} options.resolveWhenStarted
 *     Flag to indicate that the Promise has to be resolved when the
 *     page load has been started. Otherwise wait until the page has
 *     finished loading. Defaults to `false`.
 *
 * @returns {Promise}
 *     Promise which resolves when the page load is in the expected state.
 */
function waitForInitialNavigationCompleted(webProgress, options = {}) {
  const { resolveWhenStarted = false } = options;

  const browsingContext = webProgress.browsingContext;

  // Start the listener right away to avoid race conditions.
  const listener = new ProgressListener(webProgress, { resolveWhenStarted });
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

    // Will resolve the navigated promise.
    listener.stop();
  }

  return navigated;
}

/**
 * WebProgressListener to observe for page loads.
 */
class ProgressListener {
  #resolveWhenStarted;
  #unloadTimeout;

  #resolve;
  #seenStartFlag;
  #unloadTimer;
  #webProgress;

  /**
   * Create a new WebProgressListener instance.
   *
   * @param {WebProgress} webProgress
   *     The web progress to attach the listener to.
   * @param {Object=} options
   * @param {Boolean=} options.resolveWhenStarted
   *     Flag to indicate that the Promise has to be resolved when the
   *     page load has been started. Otherwise wait until the page has
   *     finished loading. Defaults to `false`.
   * @param {Number=} options.unloadTimeout
   *     Time to allow before the page gets unloaded. Defaults to 200ms.
   */
  constructor(webProgress, options = {}) {
    const { resolveWhenStarted = false, unloadTimeout = 200 } = options;

    this.#resolveWhenStarted = resolveWhenStarted;
    this.#unloadTimeout = unloadTimeout;

    this.#resolve = null;
    this.#seenStartFlag = false;
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

  #checkLoadingState(options = {}) {
    const { isStart = false, isStop = false } = options;

    if (isStart && !this.#seenStartFlag) {
      this.#seenStartFlag = true;

      logger.trace(
        truncate`[${this.browsingContext.id}] ${this.constructor.name} state=start: ${this.currentURI.spec}`
      );

      if (this.#unloadTimer) {
        this.#unloadTimer.cancel();
        this.#unloadTimer = null;
      }

      if (this.#resolveWhenStarted) {
        // Return immediately when the load should not be awaited.
        this.stop();
        return;
      }
    }

    if (isStop && this.#seenStartFlag) {
      logger.trace(
        truncate`[${this.browsingContext.id}] ${this.constructor.name} state=stop: ${this.currentURI.spec}`
      );

      this.stop();
    }
  }

  onStateChange(progress, request, flag, status) {
    this.#checkLoadingState({
      isStart: flag & Ci.nsIWebProgressListener.STATE_START,
      isStop: flag & Ci.nsIWebProgressListener.STATE_STOP,
    });
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

    if (this.#webProgress.isLoadingDocument && this.#resolveWhenStarted) {
      // Resolve immediately when the page is already loading and there
      // is no requirement to wait for it to finish.
      return Promise.resolve();
    }

    const promise = new Promise(resolve => (this.#resolve = resolve));

    // Enable all state notifications to get informed about an upcoming load
    // as early as possible.
    this.#webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_ALL
    );

    webProgressListeners.add(this);

    if (this.#webProgress.isLoadingDocument) {
      this.#checkLoadingState({ isStart: true });
    } else {
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

    this.#resolve();
    this.#resolve = null;

    this.#webProgress.removeProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_ALL
    );
    webProgressListeners.delete(this);
  }

  toString() {
    return `[object ${this.constructor.name}]`;
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([
      "nsIWebProgressListener",
      "nsISupportsWeakReference",
    ]);
  }
}
