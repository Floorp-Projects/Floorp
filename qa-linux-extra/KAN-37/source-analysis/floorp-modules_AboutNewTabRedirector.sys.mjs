/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This nsIAboutModule is for about:home and about:newtab. The primary
 * job of the AboutNewTabRedirector is to resolve requests to load about:home
 * and about:newtab to the appropriate resources for those requests.
 *
 * The AboutNewTabRedirector is not involved when the user has overridden
 * the default about:home or about:newtab pages.
 *
 * There are two implementations of this nsIAboutModule - one for the parent
 * process, and one for content processes. Each one has some secondary
 * responsibilties that are process-specific.
 *
 * The need for two implementations is an unfortunate consequence of how
 * document loading and process redirection for about: pages currently
 * works in Gecko. The commonalities between the two implementations has
 * been put into an abstract base class.
 */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { E10SUtils } from "resource://gre/modules/E10SUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BasePromiseWorker: "resource://gre/modules/PromiseWorker.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "BUILTIN_NEWTAB_ENABLED",
  "browser.newtabpage.enabled",
  true
);

/**
 * BEWARE: Do not add variables for holding state in the global scope.
 * Any state variables should be properties of the appropriate class
 * below. This is to avoid confusion where the state is set in one process,
 * but not in another.
 *
 * Constants are fine in the global scope.
 */

const PREF_ABOUT_HOME_CACHE_TESTING =
  "browser.startup.homepage.abouthome_cache.testing";

const CACHE_WORKER_URL = "resource://newtab/lib/cache.worker.js";

const IS_PRIVILEGED_PROCESS =
  Services.appinfo.remoteType === E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE;

const PREF_ACTIVITY_STREAM_DEBUG = "browser.newtabpage.activity-stream.debug";
const PREF_NEWTAB_SELF_LOADING =
  "browser.newtabpage.activity-stream.selfLoading.enabled";
const PREF_NEWTAB_REMOTE_RENDERER_ENABLED =
  "browser.newtabpage.activity-stream.remote-renderer.enabled";
/**
 * The AboutHomeStartupCacheChild is responsible for connecting the
 * AboutNewTabRedirectorChild with a cached document and script for about:home
 * if one happens to exist. The AboutHomeStartupCacheChild is only ever
 * handed the streams for those caches when the "privileged about content
 * process" first launches, so subsequent loads of about:home do not read
 * from this cache.
 *
 * See https://firefox-source-docs.mozilla.org/browser/extensions/newtab/docs/v2-system-addon/about_home_startup_cache.html
 * for further details.
 */
export const AboutHomeStartupCacheChild = {
  _initted: false,
  CACHE_REQUEST_MESSAGE: "AboutHomeStartupCache:CacheRequest",
  CACHE_RESPONSE_MESSAGE: "AboutHomeStartupCache:CacheResponse",
  CACHE_USAGE_RESULT_MESSAGE: "AboutHomeStartupCache:UsageResult",
  STATES: {
    UNAVAILABLE: 0,
    UNCONSUMED: 1,
    PAGE_CONSUMED: 2,
    PAGE_AND_SCRIPT_CONSUMED: 3,
    FAILED: 4,
    DISQUALIFIED: 5,
  },
  REQUEST_TYPE: {
    PAGE: 0,
    SCRIPT: 1,
  },
  _state: 0,
  _consumerBCID: null,

  /**
   * Called via a process script very early on in the process lifetime. This
   * prepares the AboutHomeStartupCacheChild to pass an nsIChannel back to
   * the AboutNewTabRedirectorChild when the initial about:home document is
   * eventually requested.
   *
   * @param {nsIInputStream} pageInputStream
   *   The stream for the cached page markup.
   * @param {nsIInputStream} scriptInputStream
   *   The stream for the cached script to run on the page.
   */
  init(pageInputStream, scriptInputStream) {
    if (
      !IS_PRIVILEGED_PROCESS &&
      !Services.prefs.getBoolPref(PREF_ABOUT_HOME_CACHE_TESTING, false)
    ) {
      throw new Error(
        "Can only instantiate in the privileged about content processes."
      );
    }

    if (
      !Services.prefs.getBoolPref(
        "browser.startup.homepage.abouthome_cache.enabled"
      )
    ) {
      return;
    }

    if (this._initted) {
      throw new Error("AboutHomeStartupCacheChild already initted.");
    }

    Services.obs.addObserver(this, "memory-pressure");
    Services.cpmm.addMessageListener(this.CACHE_REQUEST_MESSAGE, this);

    this._pageInputStream = pageInputStream;
    this._scriptInputStream = scriptInputStream;
    this._initted = true;
    this.setState(this.STATES.UNCONSUMED);
  },

  /**
   * A function that lets us put the AboutHomeStartupCacheChild back into
   * its initial state. This is used by tests to let us simulate the startup
   * behaviour of the module without having to manually launch a new privileged
   * about content process every time.
   */
  uninit() {
    if (!Services.prefs.getBoolPref(PREF_ABOUT_HOME_CACHE_TESTING, false)) {
      throw new Error(
        "Cannot uninit AboutHomeStartupCacheChild unless testing."
      );
    }

    if (!this._initted) {
      return;
    }

    Services.obs.removeObserver(this, "memory-pressure");
    Services.cpmm.removeMessageListener(this.CACHE_REQUEST_MESSAGE, this);

    if (this._cacheWorker) {
      this._cacheWorker.terminate();
      this._cacheWorker = null;
    }

    this._pageInputStream = null;
    this._scriptInputStream = null;
    this._initted = false;
    this._state = this.STATES.UNAVAILABLE;
    this._consumerBCID = null;
  },

  /**
   * Attempts to return an nsIChannel for a cached about:home document that
   * we were initialized with. If we failed to be initted with the cache, or the
   * input streams that we were sent have no data yet available, this function
   * returns null. The caller should fall back to generating the page
   * dynamically.
   *
   * This function will be called when loading about:home, or
   * about:home?jscache - the latter returns the cached script.
   *
   * It is expected that the same BrowsingContext that loads the cached
   * page will also load the cached script.
   *
   * @param {nsIURI} uri
   *   The URI for the requested page, as passed by AboutNewTabRedirectorChild.
   * @param {nsILoadInfo} loadInfo
   *   The nsILoadInfo for the requested load, as passed by
   *   AboutNewTabRedirectorChild.
   * @returns {?nsIChannel}
   */
  maybeGetCachedPageChannel(uri, loadInfo) {
    if (!this._initted) {
      return null;
    }

    if (this._state >= this.STATES.PAGE_AND_SCRIPT_CONSUMED) {
      return null;
    }

    let requestType =
      uri.query === "jscache"
        ? this.REQUEST_TYPE.SCRIPT
        : this.REQUEST_TYPE.PAGE;

    // If this is a page request, then we need to be in the UNCONSUMED state,
    // since we expect the page request to come first. If this is a script
    // request, we expect to be in PAGE_CONSUMED state, since the page cache
    // stream should he been consumed already.
    if (
      (requestType === this.REQUEST_TYPE.PAGE &&
        this._state !== this.STATES.UNCONSUMED) ||
      (requestType === this.REQUEST_TYPE_SCRIPT &&
        this._state !== this.STATES.PAGE_CONSUMED)
    ) {
      return null;
    }

    // If by this point, we don't have anything in the streams,
    // then either the cache was too slow to give us data, or the cache
    // doesn't exist. The caller should fall back to generating the
    // page dynamically.
    //
    // We only do this on the page request, because by the time
    // we get to the script request, we should have already drained
    // the page input stream.
    if (requestType === this.REQUEST_TYPE.PAGE) {
      try {
        if (
          !this._scriptInputStream.available() ||
          !this._pageInputStream.available()
        ) {
          this.setState(this.STATES.FAILED);
          this.reportUsageResult(false /* success */);
          return null;
        }
      } catch (e) {
        this.setState(this.STATES.FAILED);
        if (e.result === Cr.NS_BASE_STREAM_CLOSED) {
          this.reportUsageResult(false /* success */);
          return null;
        }
        throw e;
      }
    }

    if (
      requestType === this.REQUEST_TYPE.SCRIPT &&
      this._consumerBCID !== loadInfo.browsingContextID
    ) {
      // Some other document is somehow requesting the script - one
      // that didn't originally request the page. This is not allowed.
      this.setState(this.STATES.FAILED);
      return null;
    }

    let channel = Cc[
      "@mozilla.org/network/input-stream-channel;1"
    ].createInstance(Ci.nsIInputStreamChannel);
    channel.QueryInterface(Ci.nsIChannel);
    channel.setURI(uri);
    channel.loadInfo = loadInfo;
    channel.contentStream =
      requestType === this.REQUEST_TYPE.PAGE
        ? this._pageInputStream
        : this._scriptInputStream;

    if (requestType === this.REQUEST_TYPE.SCRIPT) {
      this.setState(this.STATES.PAGE_AND_SCRIPT_CONSUMED);
      this.reportUsageResult(true /* success */);
    } else {
      this.setState(this.STATES.PAGE_CONSUMED);
      // Stash the BrowsingContext ID so that when the script stream
      // attempts to be consumed, we ensure that it's from the same
      // BrowsingContext that loaded the page.
      this._consumerBCID = loadInfo.browsingContextID;
    }

    return channel;
  },

  /**
   * This function takes the state information required to generate
   * the about:home cache markup and script, and then generates that
   * markup in script asynchronously. Once that's done, a message
   * is sent to the parent process with the nsIInputStream's for the
   * markup and script contents.
   *
   * @param {object} state
   *   The Redux state of the about:home document to render.
   * @returns {Promise<undefined>}
   *   Fulfills after the message with the nsIInputStream's have been sent to
   *   the parent.
   */
  async constructAndSendCache(state) {
    if (!IS_PRIVILEGED_PROCESS) {
      throw new Error("Wrong process type.");
    }

    let worker = this.getOrCreateWorker();

    let timerId = Glean.newtab.abouthomeCacheConstruction.start();
    let direction = Services.locale.isAppLocaleRTL ? "rtl" : "ltr";

    let { page, script } = await worker
      .post("construct", [state, direction])
      .finally(() => {
        Glean.newtab.abouthomeCacheConstruction.stopAndAccumulate(timerId);
      });

    let pageInputStream = Cc[
      "@mozilla.org/io/string-input-stream;1"
    ].createInstance(Ci.nsIStringInputStream);

    pageInputStream.setUTF8Data(page);

    let scriptInputStream = Cc[
      "@mozilla.org/io/string-input-stream;1"
    ].createInstance(Ci.nsIStringInputStream);

    scriptInputStream.setUTF8Data(script);

    Services.cpmm.sendAsyncMessage(this.CACHE_RESPONSE_MESSAGE, {
      pageInputStream,
      scriptInputStream,
    });
  },

  _cacheWorker: null,
  getOrCreateWorker() {
    if (this._cacheWorker) {
      return this._cacheWorker;
    }

    this._cacheWorker = new lazy.BasePromiseWorker(CACHE_WORKER_URL);
    return this._cacheWorker;
  },

  receiveMessage(message) {
    if (message.name === this.CACHE_REQUEST_MESSAGE) {
      let { state } = message.data;
      this.constructAndSendCache(state);
    }
  },

  reportUsageResult(success) {
    Services.cpmm.sendAsyncMessage(this.CACHE_USAGE_RESULT_MESSAGE, {
      success,
    });
  },

  observe(subject, topic) {
    if (topic === "memory-pressure" && this._cacheWorker) {
      this._cacheWorker.terminate();
      this._cacheWorker = null;
    }
  },

  /**
   * Transitions the AboutHomeStartupCacheChild from one state
   * to the next, where each state is defined in this.STATES.
   *
   * States can only be transitioned in increasing order, otherwise
   * an error is logged.
   */
  setState(state) {
    if (state > this._state) {
      this._state = state;
    } else {
      console.error(
        "AboutHomeStartupCacheChild could not transition from state " +
          `${this._state} to ${state}`,
        new Error().stack
      );
    }
  },

  /**
   * If the cache hasn't been used, transitions it into the DISQUALIFIED
   * state so that it cannot be used. This should be called if it's been
   * determined that about:newtab is going to be loaded, which doesn't
   * use the cache.
   */
  disqualifyCache() {
    if (this._state === this.STATES.UNCONSUMED) {
      this.setState(this.STATES.DISQUALIFIED);
      this.reportUsageResult(false /* success */);
    }
  },
};

/**
 * This is an abstract base class for the nsIAboutModule implementations for
 * about:home and about:newtab that has some common methods and properties.
 */
class BaseAboutNewTabRedirector {
  constructor() {
    if (!AppConstants.RELEASE_OR_BETA) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "activityStreamDebug",
        PREF_ACTIVITY_STREAM_DEBUG,
        false
      );
    } else {
      this.activityStreamDebug = false;
    }

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "selfLoadingEnabled",
      PREF_NEWTAB_SELF_LOADING,
      false
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "remoteRendererEnabled",
      PREF_NEWTAB_REMOTE_RENDERER_ENABLED,
      false
    );
  }

  /**
   * @returns {string} the default URL
   *
   * This URL depends on various activity stream prefs. Overriding
   * the newtab page has no effect on the result of this function.
   */
  get defaultURL() {
    if (this.remoteRendererEnabled) {
      return "resource://newtab/data/content/remote-renderer-host.html";
    }

    // Generate the desired activity stream resource depending on state, e.g.,
    // "resource://newtab/prerendered/activity-stream.html"
    // "resource://newtab/prerendered/activity-stream-debug.html"
    // "resource://newtab/prerendered/activity-stream-noscripts.html"
    return [
      "resource://newtab/prerendered/",
      "activity-stream",
      this.activityStreamDebug && this.selfLoadingEnabled ? "-debug" : "",
      this.selfLoadingEnabled ? "" : "-noscripts",
      ".html",
    ].join("");
  }

  newChannel() {
    throw Components.Exception(
      "getChannel not implemented for this process.",
      Cr.NS_ERROR_NOT_IMPLEMENTED
    );
  }

  getURIFlags() {
    return (
      Ci.nsIAboutModule.ALLOW_SCRIPT |
      Ci.nsIAboutModule.ENABLE_INDEXED_DB |
      Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD |
      Ci.nsIAboutModule.URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS |
      Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT
    );
  }

  getChromeURI() {
    return Services.io.newURI("chrome://browser/content/blanktab.html");
  }

  QueryInterface = ChromeUtils.generateQI(["nsIAboutModule"]);
}

/**
 * The parent-process implementation of the nsIAboutModule, which is the first
 * stop for when requests are made to visit about:home or about:newtab (so
 * before the AboutNewTabRedirectorChild has a chance to handle the request).
 */
export class AboutNewTabRedirectorParent extends BaseAboutNewTabRedirector {
  #addonInitialized = false;
  #suspendedChannels = [];
  #addonInitializedPromise = null;
  #addonInitializedResolver = null;

  constructor() {
    super();

    let { promise, resolve } = Promise.withResolvers();
    this.#addonInitializedPromise = promise;
    this.#addonInitializedResolver = resolve;

    ChromeUtils.registerWindowActor("AboutNewTab", {
      parent: {
        esModuleURI: "resource:///actors/AboutNewTabParent.sys.mjs",
      },
      child: {
        esModuleURI: "resource:///actors/AboutNewTabChild.sys.mjs",
        events: {
          DOMDocElementInserted: {},
          DOMContentLoaded: { capture: true },
          load: { capture: true },
          unload: { capture: true },
          pageshow: {},
          visibilitychange: {},
        },
        observers: ["intl:l10n-sources-changed"],
      },
      // The wildcard on about:newtab is for the # parameter
      // that is used for the newtab devtools. The wildcard for about:home
      // is similar, and also allows for falling back to loading the
      // about:home document dynamically if an attempt is made to load
      // about:home?jscache from the AboutHomeStartupCache as a top-level
      // load.
      matches: ["about:home*", "about:newtab*"],
      remoteTypes: ["privilegedabout"],
    });
    this.wrappedJSObject = this;
  }

  /**
   * Waits for the AddonManager to be fully initialized, and for the built-in
   * addon to be ready. Once that's done, it tterates any suspended channels and
   * resumes them, now that the built-in addon has been set up.
   */
  notifyBuiltInAddonInitialized() {
    this.#addonInitialized = true;

    for (let suspendedChannel of this.#suspendedChannels) {
      suspendedChannel.resume();
    }
    this.#suspendedChannels = [];
    this.#addonInitializedResolver();
  }

  /**
   * Returns a Promise that resolves when the newtab built-in addon has notified
   * that it has finished initializing.
   *
   * @type {Promise<undefined>}
   */
  get promiseBuiltInAddonInitialized() {
    return this.#addonInitializedPromise;
  }

  newChannel(uri, loadInfo) {
    let chromeURI = this.getChromeURI(uri);

    if (
      uri.spec.startsWith("about:home") ||
      (uri.spec.startsWith("about:newtab") && lazy.BUILTIN_NEWTAB_ENABLED)
    ) {
      chromeURI = Services.io.newURI(this.defaultURL);
    }

    let resultChannel = Services.io.newChannelFromURIWithLoadInfo(
      chromeURI,
      loadInfo
    );
    resultChannel.originalURI = uri;

    if (!this.#addonInitialized) {
      return this.#getSuspendedChannel(resultChannel);
    }

    return resultChannel;
  }

  /**
   * Wraps an nsIChannel with an nsISuspendableChannelWrapper, suspends that
   * wrapper, and then stores the wrapper in #suspendedChannels so that it can
   * be resumed with a call to #notifyBuildInAddonInitialized.
   *
   * @param {nsIChannel} innerChannel
   *   The channel to wrap and suspend.
   * @returns {nsISuspendableChannelWrapper}
   */
  #getSuspendedChannel(innerChannel) {
    let suspendedChannel =
      Services.io.newSuspendableChannelWrapper(innerChannel);
    suspendedChannel.suspend();

    this.#suspendedChannels.push(suspendedChannel);
    return suspendedChannel;
  }
}

/**
 * The child-process implementation of nsIAboutModule, which also does the work
 * of redirecting about:home loads to the about:home startup cache if its
 * available.
 */
export class AboutNewTabRedirectorChild extends BaseAboutNewTabRedirector {
  newChannel(uri, loadInfo) {
    if (!IS_PRIVILEGED_PROCESS) {
      throw Components.Exception(
        "newChannel can only be called from the privilegedabout content process.",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    let pageURI;

    if (uri.spec.startsWith("about:home")) {
      let cacheChannel = AboutHomeStartupCacheChild.maybeGetCachedPageChannel(
        uri,
        loadInfo
      );
      if (cacheChannel) {
        return cacheChannel;
      }
      pageURI = Services.io.newURI(this.defaultURL);
    } else {
      // The only other possibility is about:newtab.
      //
      // If about:newtab is being requested, then any subsequent request for
      // about:home should _never_ request the cache (which might be woefully
      // out of date compared to about:newtab), so we disqualify the cache if
      // it still happens to be around.
      AboutHomeStartupCacheChild.disqualifyCache();

      if (lazy.BUILTIN_NEWTAB_ENABLED) {
        pageURI = Services.io.newURI(this.defaultURL);
      } else {
        pageURI = this.getChromeURI(uri);
      }
    }

    let resultChannel = Services.io.newChannelFromURIWithLoadInfo(
      pageURI,
      loadInfo
    );
    resultChannel.originalURI = uri;
    return resultChannel;
  }
}

/**
 * The AboutNewTabRedirectorStub is a function called in both the main and
 * content processes when trying to get at the nsIAboutModule for about:newtab
 * and about:home. This function does the job of choosing the appropriate
 * implementation of nsIAboutModule for the process type.
 */
export function AboutNewTabRedirectorStub() {
  if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT) {
    return new AboutNewTabRedirectorParent();
  }
  return new AboutNewTabRedirectorChild();
}
