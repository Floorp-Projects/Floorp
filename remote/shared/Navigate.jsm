/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "ProgressListener",
  "waitForInitialNavigationCompleted",
];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",

  Deferred: "chrome://remote/content/shared/Sync.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  truncate: "chrome://remote/content/shared/Format.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.REMOTE_AGENT)
);

// Define a custom multiplier to apply to the unload timer on various platforms.
// This multiplier should only reflect the navigation performance of the
// platform and not the overall performance.
XPCOMUtils.defineLazyGetter(lazy, "UNLOAD_TIMEOUT_MULTIPLIER", () => {
  if (AppConstants.MOZ_CODE_COVERAGE) {
    // Navigation on ccov platforms can be extremely slow because new processes
    // need to be instrumented for coverage on startup.
    return 16;
  }

  if (AppConstants.ASAN || AppConstants.DEBUG || AppConstants.TSAN) {
    // Use an extended timeout on slow platforms.
    return 8;
  }

  return 1;
});

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
 *     Values as returned:
 *       - {nsIURI} currentURI The current URI of the page
 *       - {nsIURI} targetURI Target URI of the navigation
 */
async function waitForInitialNavigationCompleted(webProgress, options = {}) {
  const { resolveWhenStarted = false } = options;

  const browsingContext = webProgress.browsingContext;

  // Start the listener right away to avoid race conditions.
  const listener = new ProgressListener(webProgress, {
    resolveWhenStarted,
  });
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
    lazy.logger.trace(
      lazy.truncate`[${browsingContext.id}] Document already finished loading: ${browsingContext.currentURI?.spec}`
    );

    // Will resolve the navigated promise.
    listener.stop();
  }

  await navigated;

  return {
    currentURI: listener.currentURI,
    targetURI: listener.targetURI,
  };
}

/**
 * WebProgressListener to observe for page loads.
 */
class ProgressListener {
  #expectNavigation;
  #resolveWhenStarted;
  #unloadTimeout;
  #waitForExplicitStart;
  #webProgress;

  #deferredNavigation;
  #seenStartFlag;
  #targetURI;
  #unloadTimerId;

  /**
   * Create a new WebProgressListener instance.
   *
   * @param {WebProgress} webProgress
   *     The web progress to attach the listener to.
   * @param {Object=} options
   * @param {Boolean=} options.expectNavigation
   *     Flag to indicate that a navigation is guaranteed to happen.
   *     When set to `true`, the ProgressListener will ignore options.unloadTimeout
   *     and will only resolve when the expected navigation happens.
   *     Defaults to `false`.
   * @param {Boolean=} options.resolveWhenStarted
   *     Flag to indicate that the Promise has to be resolved when the
   *     page load has been started. Otherwise wait until the page has
   *     finished loading. Defaults to `false`.
   * @param {Number=} options.unloadTimeout
   *     Time to allow before the page gets unloaded. Defaults to 200ms on
   *     regular platforms. A multiplier will be applied on slower platforms
   *     (eg. debug, ccov...).
   *     Ignored if options.expectNavigation is set to `true`
   * @param {Boolean=} options.waitForExplicitStart
   *     Flag to indicate that the Promise can only resolve after receiving a
   *     STATE_START state change. In other words, if the webProgress is already
   *     navigating, the Promise will only resolve for the next navigation.
   *     Defaults to `false`.
   */
  constructor(webProgress, options = {}) {
    const {
      expectNavigation = false,
      resolveWhenStarted = false,
      unloadTimeout = 200,
      waitForExplicitStart = false,
    } = options;

    this.#expectNavigation = expectNavigation;
    this.#resolveWhenStarted = resolveWhenStarted;
    this.#unloadTimeout = unloadTimeout * lazy.UNLOAD_TIMEOUT_MULTIPLIER;
    this.#waitForExplicitStart = waitForExplicitStart;
    this.#webProgress = webProgress;

    this.#deferredNavigation = null;
    this.#seenStartFlag = false;
    this.#targetURI = null;
    this.#unloadTimerId = null;
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

  get isStarted() {
    return !!this.#deferredNavigation;
  }

  get targetURI() {
    return this.#targetURI;
  }

  #checkLoadingState(request, options = {}) {
    const { isStart = false, isStop = false, status = 0 } = options;
    const messagePrefix = `[${this.browsingContext.id}] ${this.constructor.name}`;

    if (isStart && !this.#seenStartFlag) {
      this.#seenStartFlag = true;

      this.#targetURI = this.#getTargetURI(request);

      lazy.logger.trace(
        lazy.truncate`${messagePrefix} state=start: ${this.targetURI?.spec}`
      );

      if (this.#unloadTimerId !== null) {
        lazy.clearTimeout(this.#unloadTimerId);
        this.#unloadTimerId = null;
      }

      if (this.#resolveWhenStarted) {
        // Return immediately when the load should not be awaited.
        this.stop();
        return;
      }
    }

    if (isStop && this.#seenStartFlag) {
      // Treat NS_ERROR_PARSED_DATA_CACHED as a success code
      // since navigation happened and content has been loaded.
      if (
        !Components.isSuccessCode(status) &&
        status != Cr.NS_ERROR_PARSED_DATA_CACHED
      ) {
        // Ignore if the current navigation to the initial document is stopped
        // because the real document will be loaded instead.
        if (
          status == Cr.NS_BINDING_ABORTED &&
          this.browsingContext.currentWindowGlobal.isInitialDocument
        ) {
          return;
        }

        // The navigation request caused an error.
        const errorName = ChromeUtils.getXPCOMErrorName(status);
        lazy.logger.trace(
          lazy.truncate`${messagePrefix} state=stop: error=0x${status.toString(
            16
          )} (${errorName})`
        );
        this.stop({ error: new Error(errorName) });
        return;
      }

      lazy.logger.trace(
        lazy.truncate`${messagePrefix} state=stop: ${this.currentURI.spec}`
      );

      // If a non initial page finished loading the navigation is done.
      if (!this.browsingContext.currentWindowGlobal.isInitialDocument) {
        this.stop();
        return;
      }

      // Otherwise wait for a potential additional page load.
      lazy.logger.trace(
        `${messagePrefix} Initial document loaded. Wait for a potential further navigation.`
      );
      this.#seenStartFlag = false;
      this.#setUnloadTimer();
    }
  }

  #getTargetURI(request) {
    try {
      return request.QueryInterface(Ci.nsIChannel).originalURI;
    } catch (e) {}

    return null;
  }

  #setUnloadTimer() {
    if (!this.#expectNavigation) {
      this.#unloadTimerId = lazy.setTimeout(() => {
        lazy.logger.trace(
          lazy.truncate`[${this.browsingContext.id}] No navigation detected: ${this.currentURI?.spec}`
        );
        // Assume the target is the currently loaded URI.
        this.#targetURI = this.currentURI;
        this.stop();
      }, this.#unloadTimeout);
    }
  }

  onStateChange(progress, request, flag, status) {
    this.#checkLoadingState(request, {
      isStart: flag & Ci.nsIWebProgressListener.STATE_START,
      isStop: flag & Ci.nsIWebProgressListener.STATE_STOP,
      status,
    });
  }

  onLocationChange(progress, request, location, flag) {
    const messagePrefix = `[${this.browsingContext.id}] ${this.constructor.name}`;

    // If an error page has been loaded abort the navigation.
    if (flag & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
      lazy.logger.trace(
        lazy.truncate`${messagePrefix} location=errorPage: ${location.spec}`
      );
      this.stop({ error: new Error("Address restricted") });
      return;
    }

    // If location has changed in the same document the navigation is done.
    if (flag & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      this.#targetURI = location;
      lazy.logger.trace(
        lazy.truncate`${messagePrefix} location=sameDocument: ${this.targetURI?.spec}`
      );
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
    if (this.#deferredNavigation) {
      throw new Error(`Progress listener already started`);
    }

    if (this.#webProgress.isLoadingDocument) {
      this.#targetURI = this.#getTargetURI(this.#webProgress.documentRequest);

      if (this.#resolveWhenStarted) {
        // Resolve immediately when the page is already loading and there
        // is no requirement to wait for it to finish.
        return Promise.resolve();
      }
    }

    this.#deferredNavigation = new lazy.Deferred();

    // Enable all location change and state notifications to get informed about an upcoming load
    // as early as possible.
    this.#webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_LOCATION | Ci.nsIWebProgress.NOTIFY_STATE_ALL
    );

    webProgressListeners.add(this);

    if (this.#webProgress.isLoadingDocument && !this.#waitForExplicitStart) {
      this.#checkLoadingState(this.#webProgress.documentRequest, {
        isStart: true,
      });
    } else {
      // If the document is not loading yet wait some time for the navigation
      // to be started.
      this.#setUnloadTimer();
    }

    return this.#deferredNavigation.promise;
  }

  /**
   * Stop observing web progress changes.
   *
   * @param {Object=} options
   * @param {Error=} options.error
   *     If specified the navigation promise will be rejected with this error.
   */
  stop(options = {}) {
    const { error } = options;

    if (!this.#deferredNavigation) {
      throw new Error(`Progress listener not yet started`);
    }

    lazy.clearTimeout(this.#unloadTimerId);
    this.#unloadTimerId = null;

    this.#webProgress.removeProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_LOCATION | Ci.nsIWebProgress.NOTIFY_STATE_ALL
    );
    webProgressListeners.delete(this);

    if (!this.#targetURI) {
      // If no target URI has been set yet it should be the current URI
      this.#targetURI = this.browsingContext.currentURI;
    }

    if (error) {
      this.#deferredNavigation.reject(error);
    } else {
      this.#deferredNavigation.resolve();
    }

    this.#deferredNavigation = null;
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
