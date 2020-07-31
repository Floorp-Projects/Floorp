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
  OS: "resource://gre/modules/osfile.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchUtils",
    maxLogLevel: SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

const BROWSER_SEARCH_PREF = "browser.search.";

/**
 * Load listener
 */
class LoadListener {
  _bytes = [];
  _callback = null;
  _channel = null;
  _countRead = 0;
  _engine = null;
  _stream = null;
  QueryInterface = ChromeUtils.generateQI([
    Ci.nsIRequestObserver,
    Ci.nsIStreamListener,
    Ci.nsIChannelEventSink,
    Ci.nsIInterfaceRequestor,
    Ci.nsIProgressEventSink,
  ]);

  constructor(channel, engine, callback) {
    this._channel = channel;
    this._engine = engine;
    this._callback = callback;
  }

  // nsIRequestObserver
  onStartRequest(request) {
    logConsole.debug("loadListener: Starting request:", request.name);
    this._stream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );
  }

  onStopRequest(request, statusCode) {
    logConsole.debug("loadListener: Stopping request:", request.name);

    var requestFailed = !Components.isSuccessCode(statusCode);
    if (!requestFailed && request instanceof Ci.nsIHttpChannel) {
      requestFailed = !request.requestSucceeded;
    }

    if (requestFailed || this._countRead == 0) {
      logConsole.warn("loadListener: request failed!");
      // send null so the callback can deal with the failure
      this._bytes = null;
    }
    this._callback(this._bytes, this._engine);
    this._channel = null;
    this._engine = null;
  }

  // nsIStreamListener
  onDataAvailable(request, inputStream, offset, count) {
    this._stream.setInputStream(inputStream);

    // Get a byte array of the data
    this._bytes = this._bytes.concat(this._stream.readByteArray(count));
    this._countRead += count;
  }

  // nsIChannelEventSink
  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    this._channel = newChannel;
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  }

  // nsIInterfaceRequestor
  getInterface(iid) {
    return this.QueryInterface(iid);
  }

  // nsIProgressEventSink
  onProgress(request, progress, progressMax) {}
  onStatus(request, status, statusArg) {}
}

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

  MOZ_PARAM: {
    DATE: "moz:date",
    DIST_ID: "moz:distributionID",
    LOCALE: "moz:locale",
    OFFICIAL: "moz:official",
  },

  LoadListener,

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
        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
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

  /**
   * Sanitizes a name so that it can be used as an engine name. If it cannot be
   * sanitized (e.g. no valid characters), then it returns a random name.
   *
   * @param {string} name
   *  The name to be sanitized.
   * @returns {string}
   *  The sanitized name.
   */
  sanitizeName(name) {
    const maxLength = 60;
    const minLength = 1;
    var result = name.toLowerCase();
    result = result.replace(/\s+/g, "-");
    result = result.replace(/[^-a-z0-9]/g, "");

    // Use a random name if our input had no valid characters.
    if (result.length < minLength) {
      result = Math.random()
        .toString(36)
        .replace(/^.*\./, "");
    }

    // Force max length.
    return result.substring(0, maxLength);
  },

  getVerificationHash(name) {
    let disclaimer =
      "By modifying this file, I agree that I am doing so " +
      "only within $appName itself, using official, user-driven search " +
      "engine selection processes, and in a way which does not circumvent " +
      "user consent. I acknowledge that any attempt to change this file " +
      "from outside of $appName is a malicious act, and will be responded " +
      "to accordingly.";

    let salt =
      OS.Path.basename(OS.Constants.Path.profileDir) +
      name +
      disclaimer.replace(/\$appName/g, Services.appinfo.name);

    let converter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    // Data is an array of bytes.
    let data = converter.convertToByteArray(salt, {});
    let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
      Ci.nsICryptoHash
    );
    hasher.init(hasher.SHA256);
    hasher.update(data, data.length);

    return hasher.finish(true);
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
