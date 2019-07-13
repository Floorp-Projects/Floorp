/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

"use strict";

var EXPORTED_SYMBOLS = ["SearchUtils", "SearchExtensionLoader"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

const BROWSER_SEARCH_PREF = "browser.search.";

const EXT_SEARCH_PREFIX = "resource://search-extensions/";
const APP_SEARCH_PREFIX = "resource://search-plugins/";

// By the time we start loading an extension, it should load much
// faster than 1000ms.  This simply ensures we resolve all the
// promises and let search init complete if something happens.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "ADDON_LOAD_TIMEOUT",
  BROWSER_SEARCH_PREF + "addonLoadTimeout",
  1000
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "loggingEnabled",
  BROWSER_SEARCH_PREF + "log",
  false
);

var SearchUtils = {
  APP_SEARCH_PREFIX,

  BROWSER_SEARCH_PREF,
  EXT_SEARCH_PREFIX,
  LIST_JSON_URL:
    (AppConstants.platform == "android"
      ? APP_SEARCH_PREFIX
      : EXT_SEARCH_PREFIX) + "list.json",

  /**
   * Topic used for events involving the service itself.
   */
  TOPIC_SEARCH_SERVICE: "browser-search-service",

  // See documentation in nsISearchService.idl.
  TOPIC_ENGINE_MODIFIED: "browser-search-engine-modified",
  MODIFIED_TYPE: {
    CHANGED: "engine-changed",
    LOADED: "engine-loaded",
    REMOVED: "engine-removed",
    ADDED: "engine-added",
    DEFAULT: "engine-default",
  },

  URL_TYPE: {
    SUGGEST_JSON: "application/x-suggestions+json",
    SEARCH: "text/html",
    OPENSEARCH: "application/opensearchdescription+xml",
  },

  // The following constants are left undocumented in nsISearchService.idl
  // For the moment, they are meant for testing/debugging purposes only.

  // Set an arbitrary cap on the maximum icon size. Without this, large icons can
  // cause big delays when loading them at startup.
  MAX_ICON_SIZE: 20000,

  // Default charset to use for sending search parameters. ISO-8859-1 is used to
  // match previous nsInternetSearchService behavior as a URL parameter. Label
  // resolution causes windows-1252 to be actually used.
  DEFAULT_QUERY_CHARSET: "ISO-8859-1",

  /**
   * This is the Remote Settings key that we use to get the ignore lists for
   * engines.
   */
  SETTINGS_IGNORELIST_KEY: "hijack-blocklists",

  /**
   * Notifies watchers of SEARCH_ENGINE_TOPIC about changes to an engine or to
   * the state of the search service.
   *
   * @param {nsISearchEngine} engine
   *   The engine to which the change applies.
   * @param {string} verb
   *   A verb describing the change.
   *
   * @see nsISearchService.idl
   */
  notifyAction(engine, verb) {
    if (Services.search.isInitialized) {
      this.log('NOTIFY: Engine: "' + engine.name + '"; Verb: "' + verb + '"');
      Services.obs.notifyObservers(engine, this.TOPIC_ENGINE_MODIFIED, verb);
    }
  },

  /**
   * Outputs text to the JavaScript console as well as to stdout.
   *
   * @param {string} text
   *   The message to log.
   */
  log(text) {
    if (loggingEnabled) {
      Services.console.logStringMessage(text);
    }
  },

  /**
   * Logs the failure message (if browser.search.log is enabled) and throws.
   * @param {string} message
   *   A message to display
   * @param {number} resultCode
   *   The NS_ERROR_* value to throw.
   * @throws resultCode or NS_ERROR_INVALID_ARG if resultCode isn't specified.
   */
  fail(message, resultCode) {
    this.log(message);
    throw Components.Exception(message, resultCode || Cr.NS_ERROR_INVALID_ARG);
  },

  /**
   * Wrapper function for nsIIOService::newURI.
   * @param {string} urlSpec
   *        The URL string from which to create an nsIURI.
   * @returns {nsIURI} an nsIURI object, or null if the creation of the URI failed.
   */
  makeURI(urlSpec) {
    try {
      return Services.io.newURI(urlSpec);
    } catch (ex) {}

    return null;
  },

  /**
   * Wrapper function for nsIIOService::newChannel.
   *
   * @param {string|nsIURI} url
   *   The URL string from which to create an nsIChannel.
   * @returns {nsIChannel}
   *   an nsIChannel object, or null if the url is invalid.
   */
  makeChannel(url) {
    try {
      let uri = typeof url == "string" ? Services.io.newURI(url) : url;
      return Services.io.newChannelFromURI(
        uri,
        null /* loadingNode */,
        Services.scriptSecurityManager.getSystemPrincipal(),
        null /* triggeringPrincipal */,
        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
        Ci.nsIContentPolicy.TYPE_OTHER
      );
    } catch (ex) {}

    return null;
  },

  makeExtensionId(name) {
    return name + "@search.mozilla.org";
  },

  getExtensionUrl(id) {
    return EXT_SEARCH_PREFIX + id.split("@")[0] + "/";
  },
};

/**
 * SearchExtensionLoader provides a simple install function that
 * returns a set of promises.  The caller (SearchService) must resolve
 * each extension id once it has handled the final part of the install
 * (creating the SearchEngine).  Once they are resolved, the extensions
 * are fully functional, in terms of the SearchService, and initialization
 * can be completed.
 *
 * When an extension is installed (that has a search provider), the
 * extension system will call ss.addEnginesFromExtension. When that is
 * completed, SearchService calls back to resolve the promise.
 */
const SearchExtensionLoader = {
  _promises: new Map(),
  // strict is used in tests, causes load errors to reject.
  _strict: false,
  // chaos mode is used in search config tests.  It bypasses
  // reloading extensions, otherwise over the course of these
  // tests we do over 700K reloads of extensions.
  _chaosMode: false,

  /**
   * Creates a deferred promise for an extension install.
   * @param {string} id the extension id.
   * @returns {Promise}
   */
  _addPromise(id) {
    let deferred = PromiseUtils.defer();
    // We never want to have some uncaught problem stop the SearchService
    // init from completing, so timeout the promise.
    if (ADDON_LOAD_TIMEOUT > 0) {
      deferred.timeout = setTimeout(() => {
        deferred.reject(id, new Error("addon install timed out."));
        this._promises.delete(id);
      }, ADDON_LOAD_TIMEOUT);
    }
    this._promises.set(id, deferred);
    return deferred.promise;
  },

  /**
   * @param {string} id the extension id to resolve.
   */
  resolve(id) {
    if (this._promises.has(id)) {
      let deferred = this._promises.get(id);
      if (deferred.timeout) {
        clearTimeout(deferred.timeout);
      }
      deferred.resolve();
      this._promises.delete(id);
    }
  },

  /**
   * @param {string} id the extension id to reject.
   * @param {object} error The error to log when rejecting.
   */
  reject(id, error) {
    if (this._promises.has(id)) {
      let deferred = this._promises.get(id);
      if (deferred.timeout) {
        clearTimeout(deferred.timeout);
      }
      // We don't want to reject here because that will reject the promise.all
      // and stop the searchservice init.  Log the error, and resolve the promise.
      // strict mode can be used by tests to force an exception to occur.
      Cu.reportError(`Addon install for search engine ${id} failed: ${error}`);
      if (this._strict) {
        deferred.reject();
      } else {
        deferred.resolve();
      }
      this._promises.delete(id);
    }
  },

  _reset() {
    SearchUtils.log(`SearchExtensionLoader.reset`);
    for (let id of this._promises.keys()) {
      this.reject(id, new Error(`installAddons reset during install`));
    }
    this._promises = new Map();
  },

  /**
   * Tell AOM to install a set of built-in extensions.  If the extension is
   * already installed, it will be reinstalled.
   *
   * @param {Array} engineIDList is an array of extension IDs.
   * @returns {Promise} resolved when all engines have finished installation.
   */
  async installAddons(engineIDList) {
    SearchUtils.log(`SearchExtensionLoader.installAddons`);
    // If SearchService calls us again, it is being re-inited.  reset ourselves.
    this._reset();
    let promises = [];
    for (let id of engineIDList) {
      promises.push(this._addPromise(id));
      let path = SearchUtils.getExtensionUrl(id);
      SearchUtils.log(
        `SearchExtensionLoader.installAddons: installing ${id} at ${path}`
      );
      if (this._chaosMode) {
        // If the extension is already loaded, we do not reload the extension.  Instead
        // we call back to search service directly.
        let policy = WebExtensionPolicy.getByID(id);
        if (policy) {
          Services.search.addEnginesFromExtension(policy.extension);
          continue;
        }
      }
      // The AddonManager will install the engine asynchronously
      AddonManager.installBuiltinAddon(path).catch(error => {
        // Catch any install errors and propogate.
        this.reject(id, error);
      });
    }

    return Promise.all(promises);
  },
};
