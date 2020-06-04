/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

"use strict";

var EXPORTED_SYMBOLS = ["SearchUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchUtils",
    maxLogLevel: SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

const BROWSER_SEARCH_PREF = "browser.search.";

var SearchUtils = {
  BROWSER_SEARCH_PREF,

  SETTINGS_KEY: "search-config",

  /**
   * This is the Remote Settings key that we use to get the ignore lists for
   * engines.
   */
  SETTINGS_IGNORELIST_KEY: "hijack-blocklists",

  /**
   * This is the Remote Settings key that we use to get the allow lists for
   * overriding the default engines.
   */
  SETTINGS_ALLOWLIST_KEY: "search-default-override-allowlist",

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
    DEFAULT_PRIVATE: "engine-default-private",
  },

  URL_TYPE: {
    SUGGEST_JSON: "application/x-suggestions+json",
    SEARCH: "text/html",
    OPENSEARCH: "application/opensearchdescription+xml",
  },

  ENGINES_URLS: {
    "prod-main":
      "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-config/records",
    "prod-preview":
      "https://firefox.settings.services.mozilla.com/v1/buckets/main-preview/collections/search-config/records",
    "stage-main":
      "https://settings.stage.mozaws.net/v1/buckets/main/collections/search-config/records",
    "stage-preview":
      "https://settings.stage.mozaws.net/v1/buckets/main-preview/collections/search-config/records",
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

  // A tag to denote when we are using the "default_locale" of an engine.
  DEFAULT_TAG: "default",

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
      logConsole.debug("NOTIFY: Engine:", engine.name, "Verb:", verb);
      Services.obs.notifyObservers(engine, this.TOPIC_ENGINE_MODIFIED, verb);
    }
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

  /**
   * Tests whether this a partner distribution.
   *
   * @returns {boolean}
   *   Whether this is a partner distribution.
   */
  isPartnerBuild() {
    return SearchUtils.distroID && !SearchUtils.distroID.startsWith("mozilla");
  },

  /**
   * Current cache version. This should be incremented if the format of the cache
   * file is modified.
   *
   * @returns {number}
   *   The current cache version.
   */
  get CACHE_VERSION() {
    return this.gModernConfig ? 5 : 3;
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  SearchUtils,
  "gModernConfig",
  SearchUtils.BROWSER_SEARCH_PREF + "modernConfig",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  SearchUtils,
  "loggingEnabled",
  BROWSER_SEARCH_PREF + "log",
  false
);

// Can't use defineLazyPreferenceGetter because we want the value
// from the default branch
XPCOMUtils.defineLazyGetter(SearchUtils, "distroID", () => {
  return Services.prefs.getDefaultBranch("distribution.").getCharPref("id", "");
});
