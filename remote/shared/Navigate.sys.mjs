/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",

  Deferred: "chrome://remote/content/shared/Sync.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  truncate: "chrome://remote/content/shared/Format.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.REMOTE_AGENT)
);

// Define a custom multiplier to apply to the unload timer on various platforms.
// This multiplier should only reflect the navigation performance of the
// platform and not the overall performance.
ChromeUtils.defineLazyGetter(lazy, "UNLOAD_TIMEOUT_MULTIPLIER", () => {
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

export const DEFAULT_UNLOAD_TIMEOUT = 200;

// Load flag for an error page from the DocShell (0x0001U << 16)
const LOAD_FLAG_ERROR_PAGE = 0x10000;

const STATE_START = Ci.nsIWebProgressListener.STATE_START;
const STATE_STOP = Ci.nsIWebProgressListener.STATE_STOP;

/**
 * Returns the multiplier used for the unload timer. Useful for tests which
 * assert the behavior of this timeout.
 */
export function getUnloadTimeoutMultiplier() {
  return lazy.UNLOAD_TIMEOUT_MULTIPLIER;
}

// Used to keep weak references of webProgressListeners alive.
const webProgressListeners = new Set();

/**
 * Wait until the initial load of the given WebProgress is done.
 *
 * @param {WebProgress} webProgress
 *     The WebProgress instance to observe.
 * @param {object=} options
 * @param {boolean=} options.resolveWhenStarted
 *     Flag to indicate that the Promise has to be resolved when the
 *     page load has been started. Otherwise wait until the page has
 *     finished loading. Defaults to `false`.
 * @param {number=} options.unloadTimeout
 *     Time to allow before the page gets unloaded. See ProgressListener options.
 * @returns {Promise}
 *     Promise which resolves when the page load is in the expected state.
 *     Values as returned:
 *       - {nsIURI} currentURI The current URI of the page
 *       - {nsIURI} targetURI Target URI of the navigation
 */
export async function waitForInitialNavigationCompleted(
  webProgress,
  options = {}
) {
  const { resolveWhenStarted = false, unloadTimeout } = options;

  const browsingContext = webProgress.browsingContext;

  // Start the listener right away to avoid race conditions.
  const listener = new ProgressListener(webProgress, {
    resolveWhenStarted,
    unloadTimeout,
  });
  const navigated = listener.start();

  // Right after a browsing context has been attached it could happen that
  // no window global has been set yet. Consider this as nothing has been
  // loaded yet.
  let isInitial = true;
  if (browsingContext.currentWindowGlobal) {
    isInitial = browsingContext.currentWindowGlobal.isInitialDocument;
  }

  const isLoadingDocument = listener.isLoadingDocument;
  lazy.logger.trace(
    lazy.truncate`[${browsingContext.id}] Wait for initial navigation: isInitial=${isInitial}, isLoadingDocument=${isLoadingDocument}`
  );

  // If the current document is not the initial "about:blank" and is also
  // no longer loading, assume the navigation is done and return.
  if (!isInitial && !isLoadingDocument) {
    lazy.logger.trace(
      lazy.truncate`[${browsingContext.id}] Document already finished loading: ${browsingContext.currentURI?.spec}`
    );

    // Will resolve the navigated promise.
    listener.stop();
  }

  try {
    await navigated;
  } catch (e) {
    // Ignore any error if the initial navigation failed.
    lazy.logger.debug(
      lazy.truncate`[${browsingContext.id}] Initial Navigation to ${listener.currentURI?.spec} failed: ${e}`
    );
  }

  return {
    currentURI: listener.currentURI,
    targetURI: listener.targetURI,
  };
}

/**
 * WebProgressListener to observe for page loads.
 */
export class ProgressListener {
  #expectNavigation;
  #resolveWhenStarted;
  #unloadTimeout;
  #waitForExplicitStart;
  #webProgress;

  #deferredNavigation;
  #errorName;
  #seenStartFlag;
  #targetURI;
  #unloadTimerId;

  /**
   * Create a new WebProgressListener instance.
   *
   * @param {WebProgress} webProgress
   *     The web progress to attach the listener to.
   * @param {object=} options
   * @param {boolean=} options.expectNavigation
   *     Flag to indicate that a navigation is guaranteed to happen.
   *     When set to `true`, the ProgressListener will ignore options.unloadTimeout
   *     and will only resolve when the expected navigation happens.
   *     Defaults to `false`.
   * @param {boolean=} options.resolveWhenStarted
   *     Flag to indicate that the Promise has to be resolved when the
   *     page load has been started. Otherwise wait until the page has
   *     finished loading. Defaults to `false`.
   * @param {number=} options.unloadTimeout
   *     Time to allow before the page gets unloaded. Defaults to 200ms on
   *     regular platforms. A multiplier will be applied on slower platforms
   *     (eg. debug, ccov...).
   *     Ignored if options.expectNavigation is set to `true`
   * @param {boolean=} options.waitForExplicitStart
   *     Flag to indicate that the Promise can only resolve after receiving a
   *     STATE_START state change. In other words, if the webProgress is already
   *     navigating, the Promise will only resolve for the next navigation.
   *     Defaults to `false`.
   */
  constructor(webProgress, options = {}) {
    const {
      expectNavigation = false,
      resolveWhenStarted = false,
      unloadTimeout = DEFAULT_UNLOAD_TIMEOUT,
      waitForExplicitStart = false,
    } = options;

    this.#expectNavigation = expectNavigation;
    this.#resolveWhenStarted = resolveWhenStarted;
    this.#unloadTimeout = unloadTimeout * lazy.UNLOAD_TIMEOUT_MULTIPLIER;
    this.#waitForExplicitStart = waitForExplicitStart;
    this.#webProgress = webProgress;

    this.#deferredNavigation = null;
    this.#errorName = null;
    this.#seenStartFlag = false;
    this.#targetURI = null;
    this.#unloadTimerId = null;
  }

  get #messagePrefix() {
    return `[${this.browsingContext.id}] ${this.constructor.name}`;
  }

  get browsingContext() {
    return this.#webProgress.browsingContext;
  }

  get currentURI() {
    return this.#webProgress.browsingContext.currentURI;
  }

  get documentURI() {
    return this.#webProgress.browsingContext.currentWindowGlobal.documentURI;
  }

  get isInitialDocument() {
    return this.#webProgress.browsingContext.currentWindowGlobal
      .isInitialDocument;
  }

  get isLoadingDocument() {
    return this.#webProgress.isLoadingDocument;
  }

  get isStarted() {
    return !!this.#deferredNavigation;
  }

  get loadType() {
    return this.#webProgress.loadType;
  }

  get targetURI() {
    return this.#targetURI;
  }

  #checkLoadingState(request, options = {}) {
    const { isStart = false, isStop = false, status = 0 } = options;

    this.#trace(
      `Loading state: isStart=${isStart} isStop=${isStop} status=0x${status.toString(
        16
      )}, loadType=0x${this.loadType.toString(16)}`
    );
    if (isStart && !this.#seenStartFlag) {
      this.#seenStartFlag = true;

      this.#targetURI = this.#getTargetURI(request);

      this.#trace(lazy.truncate`Started loading ${this.targetURI?.spec}`);

      if (this.#unloadTimerId !== null) {
        lazy.clearTimeout(this.#unloadTimerId);
        this.#trace("Cleared the unload timer");
        this.#unloadTimerId = null;
      }

      if (this.#resolveWhenStarted) {
        this.#trace("Request to stop listening when navigation started");
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
        const errorName = ChromeUtils.getXPCOMErrorName(status);

        if (this.loadType & LOAD_FLAG_ERROR_PAGE) {
          // Wait for the next location change notification to ensure that the
          // real error page was loaded.
          this.#trace(`Error=${errorName}, wait for redirect to error page`);
          this.#errorName = errorName;
          return;
        }

        // Handle an aborted navigation. While for an initial document another
        // navigation to the real document will happen it's not the case for
        // normal documents. Here we need to stop the listener immediately.
        if (status == Cr.NS_BINDING_ABORTED && this.isInitialDocument) {
          this.#trace(
            "Ignore aborted navigation error to the initial document."
          );
          return;
        }

        this.stop({ error: new Error(errorName) });
        return;
      }

      // If a non initial page finished loading the navigation is done.
      if (!this.isInitialDocument) {
        this.stop();
        return;
      }

      // Otherwise wait for a potential additional page load.
      this.#trace(
        "Initial document loaded. Wait for a potential further navigation."
      );
      this.#seenStartFlag = false;
      this.#setUnloadTimer();
    }
  }

  #getErrorName(documentURI) {
    try {
      // Otherwise try to retrieve it from the document URI if it is an
      // error page like `about:neterror?e=contentEncodingError&u=http%3A//...`
      const regex = /about:.*error\?e=([^&]*)/;
      return documentURI.spec.match(regex)[1];
    } catch (e) {
      // Or return a generic name
      return "Address rejected";
    }
  }

  #getTargetURI(request) {
    try {
      return request.QueryInterface(Ci.nsIChannel).originalURI;
    } catch (e) {}

    return null;
  }

  #setUnloadTimer() {
    if (this.#expectNavigation) {
      this.#trace("Skip setting the unload timer");
    } else {
      this.#trace(`Setting unload timer (${this.#unloadTimeout}ms)`);

      this.#unloadTimerId = lazy.setTimeout(() => {
        this.#trace(`No navigation detected: ${this.currentURI?.spec}`);
        // Assume the target is the currently loaded URI.
        this.#targetURI = this.currentURI;
        this.stop();
      }, this.#unloadTimeout);
    }
  }

  #trace(message) {
    lazy.logger.trace(lazy.truncate`${this.#messagePrefix} ${message}`);
  }

  onStateChange(progress, request, flag, status) {
    this.#checkLoadingState(request, {
      isStart: !!(flag & STATE_START),
      isStop: !!(flag & STATE_STOP),
      status,
    });
  }

  onLocationChange(progress, request, location, flag) {
    // If an error page has been loaded abort the navigation.
    if (flag & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
      const errorName = this.#errorName || this.#getErrorName(this.documentURI);
      this.#trace(
        lazy.truncate`Location=errorPage, error=${errorName}, url=${this.documentURI.spec}`
      );
      this.stop({ error: new Error(errorName) });
      return;
    }

    // If location has changed in the same document the navigation is done.
    if (flag & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      this.#targetURI = location;
      this.#trace(`Location=sameDocument: ${this.targetURI?.spec}`);
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

    this.#trace(
      `Start: expectNavigation=${this.#expectNavigation} resolveWhenStarted=${
        this.#resolveWhenStarted
      } unloadTimeout=${this.#unloadTimeout} waitForExplicitStart=${
        this.#waitForExplicitStart
      }`
    );

    if (this.#webProgress.isLoadingDocument) {
      this.#targetURI = this.#getTargetURI(this.#webProgress.documentRequest);
      this.#trace(`Document already loading ${this.#targetURI?.spec}`);

      if (this.#resolveWhenStarted && !this.#waitForExplicitStart) {
        this.#trace(
          "Resolve on document loading if not waiting for a load or a new navigation"
        );
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
   * @param {object=} options
   * @param {Error=} options.error
   *     If specified the navigation promise will be rejected with this error.
   */
  stop(options = {}) {
    const { error } = options;

    this.#trace(
      lazy.truncate`Stop: has error=${!!error} url=${this.currentURI.spec}`
    );

    if (!this.#deferredNavigation) {
      throw new Error("Progress listener not yet started");
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

  /**
   * Stop the progress listener if and only if we already detected a navigation
   * start.
   *
   * @param {object=} options
   * @param {Error=} options.error
   *     If specified the navigation promise will be rejected with this error.
   */
  stopIfStarted(options) {
    this.#trace(`Stop if started: seenStartFlag=${this.#seenStartFlag}`);
    if (this.#seenStartFlag) {
      this.stop(options);
    }
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
