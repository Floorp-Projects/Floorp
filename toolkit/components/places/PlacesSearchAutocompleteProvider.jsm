/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Provides functions to handle search engine URLs in the browser history.
 */

"use strict";

this.EXPORTED_SYMBOLS = [ "PlacesSearchAutocompleteProvider" ];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SearchSuggestionController",
  "resource://gre/modules/SearchSuggestionController.jsm");

const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

const SearchAutocompleteProviderInternal = {
  /**
   * Array of objects in the format returned by findMatchByToken.
   */
  priorityMatches: null,

  /**
   * Array of objects in the format returned by findMatchByAlias.
   */
  aliasMatches: null,

  /**
   * Object for the default search match.
   **/
  defaultMatch: null,

  initialize: function () {
    return new Promise((resolve, reject) => {
      Services.search.init(status => {
        if (!Components.isSuccessCode(status)) {
          reject(new Error("Unable to initialize search service."));
        }

        try {
          // The initial loading of the search engines must succeed.
          this._refresh();

          Services.obs.addObserver(this, SEARCH_ENGINE_TOPIC, true);

          this.initialized = true;
          resolve();
        } catch (ex) {
          reject(ex);
        }
      });
    });
  },

  initialized: false,

  observe: function (subject, topic, data) {
    switch (data) {
      case "engine-added":
      case "engine-changed":
      case "engine-removed":
      case "engine-current":
        this._refresh();
    }
  },

  _refresh: function () {
    this.priorityMatches = [];
    this.aliasMatches = [];
    this.defaultMatch = null;

    let currentEngine = Services.search.currentEngine;
    // This can be null in XCPShell.
    if (currentEngine) {
      this.defaultMatch = {
        engineName: currentEngine.name,
        iconUrl: currentEngine.iconURI ? currentEngine.iconURI.spec : null,
      }
    }

    // The search engines will always be processed in the order returned by the
    // search service, which can be defined by the user.
    Services.search.getVisibleEngines().forEach(e => this._addEngine(e));
  },

  _addEngine: function (engine) {
    if (engine.alias) {
      this.aliasMatches.push({
        alias: engine.alias,
        engineName: engine.name,
        iconUrl: engine.iconURI ? engine.iconURI.spec : null,
      });
    }

    let domain = engine.getResultDomain();
    if (domain) {
      this.priorityMatches.push({
        token: domain,
        // The searchForm property returns a simple URL for the search engine, but
        // we may need an URL which includes an affiliate code (bug 990799).
        url: engine.searchForm,
        engineName: engine.name,
        iconUrl: engine.iconURI ? engine.iconURI.spec : null,
      });
    }
  },

  getSuggestionController(searchToken, inPrivateContext, maxResults, userContextId) {
    let engine = Services.search.currentEngine;
    if (!engine) {
      return null;
    }
    return new SearchSuggestionControllerWrapper(engine, searchToken,
                                                 inPrivateContext, maxResults,
                                                 userContextId);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),
}

function SearchSuggestionControllerWrapper(engine, searchToken,
                                           inPrivateContext, maxResults,
                                           userContextId) {
  this._controller = new SearchSuggestionController();
  this._controller.maxLocalResults = 0;
  this._controller.maxRemoteResults = maxResults;
  let promise = this._controller.fetch(searchToken, inPrivateContext, engine, userContextId);
  this._suggestions = [];
  this._success = false;
  this._promise = promise.then(results => {
    this._success = true;
    this._suggestions = (results ? results.remote : null) || [];
  }).catch(err => {
    // fetch() rejects its promise if there's a pending request.
  });
}

SearchSuggestionControllerWrapper.prototype = {

  /**
   * Resolved when all suggestions have been fetched.
   */
  get fetchCompletePromise() {
    return this._promise;
  },

  /**
   * Returns one suggestion, if any are available.  The returned value is an
   * array [match, suggestion].  If none are available, returns [null, null].
   * Note that there are two reasons that suggestions might not be available:
   * all suggestions may have been fetched and consumed, or the fetch may not
   * have completed yet.
   *
   * @return An array [match, suggestion].
   */
  consume() {
    return !this._suggestions.length ? [null, null] :
           [SearchAutocompleteProviderInternal.defaultMatch,
            this._suggestions.shift()];
  },

  /**
   * Returns the number of fetched suggestions, or -1 if the fetching was
   * incomplete or failed.
   */
  get resultsCount() {
    return this._success ? this._suggestions.length : -1;
  },

  /**
   * Stops the fetch.
   */
  stop() {
    this._controller.stop();
  },
};

var gInitializationPromise = null;

this.PlacesSearchAutocompleteProvider = Object.freeze({
  /**
   * Starts initializing the component and returns a promise that is resolved or
   * rejected when initialization finished.  The same promise is returned if
   * this function is called multiple times.
   */
  ensureInitialized: function () {
    if (!gInitializationPromise) {
      gInitializationPromise = SearchAutocompleteProviderInternal.initialize();
    }
    return gInitializationPromise;
  },

  /**
   * Matches a given string to an item that should be included by URL search
   * components, like autocomplete in the address bar.
   *
   * @param searchToken
   *        String containing the first part of the matching domain name.
   *
   * @return An object with the following properties, or undefined if the token
   *         does not match any relevant URL:
   *         {
   *           token: The full string used to match the search term to the URL.
   *           url: The URL to navigate to if the match is selected.
   *           engineName: The display name of the search engine.
   *           iconUrl: Icon associated to the match, or null if not available.
   *         }
   */
  findMatchByToken: Task.async(function* (searchToken) {
    yield this.ensureInitialized();

    // Match at the beginning for now.  In the future, an "options" argument may
    // allow the matching behavior to be tuned.
    return SearchAutocompleteProviderInternal.priorityMatches
                                             .find(m => m.token.startsWith(searchToken));
  }),

  /**
   * Matches a given search string to an item that should be included by
   * components wishing to search using search engine aliases, like
   * autocomple.
   *
   * @param searchToken
   *        Search string to match exactly a search engine alias.
   *
   * @return An object with the following properties, or undefined if the token
   *         does not match any relevant URL:
   *         {
   *           alias: The matched search engine's alias.
   *           engineName: The display name of the search engine.
   *           iconUrl: Icon associated to the match, or null if not available.
   *         }
   */
  findMatchByAlias: Task.async(function* (searchToken) {
    yield this.ensureInitialized();

    return SearchAutocompleteProviderInternal.aliasMatches
             .find(m => m.alias.toLocaleLowerCase() == searchToken.toLocaleLowerCase());
  }),

  getDefaultMatch: Task.async(function* () {
    yield this.ensureInitialized();

    return SearchAutocompleteProviderInternal.defaultMatch;
  }),

  /**
   * Synchronously determines if the provided URL represents results from a
   * search engine, and provides details about the match.
   *
   * @param url
   *        String containing the URL to parse.
   *
   * @return An object with the following properties, or null if the URL does
   *         not represent a search result:
   *         {
   *           engineName: The display name of the search engine.
   *           terms: The originally sought terms extracted from the URI.
   *         }
   *
   * @remarks The asynchronous ensureInitialized function must be called before
   *          this synchronous method can be used.
   *
   * @note This API function needs to be synchronous because it is called inside
   *       a row processing callback of Sqlite.jsm, in UnifiedComplete.js.
   */
  parseSubmissionURL: function (url) {
    if (!SearchAutocompleteProviderInternal.initialized) {
      throw new Error("The component has not been initialized.");
    }

    let parseUrlResult = Services.search.parseSubmissionURL(url);
    return parseUrlResult.engine && {
      engineName: parseUrlResult.engine.name,
      terms: parseUrlResult.terms,
    };
  },

  getSuggestionController(searchToken, inPrivateContext, maxResults, userContextId) {
    if (!SearchAutocompleteProviderInternal.initialized) {
      throw new Error("The component has not been initialized.");
    }
    return SearchAutocompleteProviderInternal.getSuggestionController(
             searchToken, inPrivateContext, maxResults, userContextId);
  },
});
