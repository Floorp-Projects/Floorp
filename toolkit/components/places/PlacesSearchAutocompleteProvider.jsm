/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Provides functions to handle search engine URLs in the browser history.
 */

"use strict";

var EXPORTED_SYMBOLS = [ "PlacesSearchAutocompleteProvider" ];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "SearchSuggestionController",
  "resource://gre/modules/SearchSuggestionController.jsm");

const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

const SearchAutocompleteProviderInternal = {
  /**
   * {Map<string: nsISearchEngine>} Maps from each domain to the engine with
   * that domain.  If more than one engine has the same domain, the last one
   * passed to _addEngine will be the one in this map.
   */
  enginesByDomain: new Map(),

  /**
   * {Map<string: nsISearchEngine>} Maps from each lowercased alias to the
   * engine with that alias.  If more than one engine has the same alias, the
   * last one passed to _addEngine will be the one in this map.
   */
  enginesByAlias: new Map(),

  /**
   * {array<{ {nsISearchEngine} engine, {array<string>} tokenAliases }>} Array
   * of engines that have "@" aliases.
   */
  tokenAliasEngines: [],

  async initialize() {
    try {
      await Services.search.init();
    } catch (errorCode) {
      throw new Error("Unable to initialize search service.");
    }

    // The initial loading of the search engines must succeed.
    await this._refresh();

    Services.obs.addObserver(this, SEARCH_ENGINE_TOPIC, true);

    this.initialized = true;
  },

  initialized: false,

  observe(subject, topic, data) {
    switch (data) {
      case "engine-added":
      case "engine-changed":
      case "engine-removed":
      case "engine-default":
        this._refresh();
    }
  },

  async _refresh() {
    this.enginesByDomain.clear();
    this.enginesByAlias.clear();
    this.tokenAliasEngines = [];

    // The search engines will always be processed in the order returned by the
    // search service, which can be defined by the user.
    (await Services.search.getEngines()).forEach(e => this._addEngine(e));
  },

  _addEngine(engine) {
    let domain = engine.getResultDomain();
    if (domain && !engine.hidden) {
      this.enginesByDomain.set(domain, engine);
    }

    let aliases = [];
    if (engine.alias) {
      aliases.push(engine.alias);
    }
    aliases.push(...engine.wrappedJSObject._internalAliases);
    for (let alias of aliases) {
      this.enginesByAlias.set(alias.toLocaleLowerCase(), engine);
    }

    let tokenAliases = aliases.filter(a => a.startsWith("@"));
    if (tokenAliases.length) {
      this.tokenAliasEngines.push({ engine, tokenAliases });
    }
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),
};

class SuggestionsFetch {
  /**
   * Create a new instance of this class for each new suggestions fetch.
   *
   * @param {nsISearchEngine} engine
   *        The engine from which suggestions will be fetched.
   * @param {string} searchString
   *        The search query string.
   * @param {bool} inPrivateContext
   *        Pass true if the fetch is being done in a private window.
   * @param {int} maxLocalResults
   *        The maximum number of results to fetch from the user's local
   *        history.
   * @param {int} maxRemoteResults
   *        The maximum number of results to fetch from the search engine.
   * @param {int} userContextId
   *        The user context ID in which the fetch is being performed.
   */
  constructor(engine,
              searchString,
              inPrivateContext,
              maxLocalResults,
              maxRemoteResults,
              userContextId) {
    this._controller = new SearchSuggestionController();
    this._controller.maxLocalResults = maxLocalResults;
    this._controller.maxRemoteResults = maxRemoteResults;
    this._engine = engine;
    this._suggestions = [];
    this._success = false;
    this._promise = this._controller.fetch(searchString, inPrivateContext, engine, userContextId).then(results => {
      this._success = true;
      if (results) {
        this._suggestions.push(
          ...results.local.map(r => ({ suggestion: r, historical: true })),
          ...results.remote.map(r => ({ suggestion: r, historical: false }))
        );
      }
    }).catch(err => {
      // fetch() rejects its promise if there's a pending request.
    });
  }

  /**
   * {nsISearchEngine} The engine from which suggestions are being fetched.
   */
  get engine() {
    return this._engine;
  }

  /**
   * {promise} Resolved when all suggestions have been fetched.
   */
  get fetchCompletePromise() {
    return this._promise;
  }

  /**
   * Returns one suggestion, if any are available, otherwise returns null.
   * Note that may be multiple reasons why suggestions are not available:
   *  - all suggestions have already been consumed
   *  - the fetch failed
   *  - the fetch didn't complete yet (should have awaited the promise)
   *
   * @returns {object} An object { suggestion, historical } or null if no
   *          suggestions are available.
   *          - suggestion {string} The suggestion.
   *          - historical {bool} True if the suggestion comes from the user's
   *            local history (instead of the search engine).
   */
  consume() {
    return this._suggestions.shift() || null;
  }

  /**
   * Returns the number of fetched suggestions, or -1 if the fetching was
   * incomplete or failed.
   */
  get resultsCount() {
    return this._success ? this._suggestions.length : -1;
  }

  /**
   * Stops the fetch.
   */
  stop() {
    this._controller.stop();
  }
}

var gInitializationPromise = null;

var PlacesSearchAutocompleteProvider = Object.freeze({
  /**
   * Starts initializing the component and returns a promise that is resolved or
   * rejected when initialization finished.  The same promise is returned if
   * this function is called multiple times.
   */
  ensureInitialized() {
    if (!gInitializationPromise) {
      gInitializationPromise = SearchAutocompleteProviderInternal.initialize();
    }
    return gInitializationPromise;
  },

  /**
   * Gets the engine whose domain matches a given prefix.
   *
   * @param   {string} prefix
   *          String containing the first part of the matching domain name.
   * @returns {nsISearchEngine} The matching engine or null if there isn't one.
   */
  async engineForDomainPrefix(prefix) {
    await this.ensureInitialized();

    // Match at the beginning for now.  In the future, an "options" argument may
    // allow the matching behavior to be tuned.
    let tuples = SearchAutocompleteProviderInternal.enginesByDomain.entries();
    for (let [domain, engine] of tuples) {
      if (domain.startsWith(prefix) || domain.startsWith("www." + prefix)) {
        return engine;
      }
    }
    return null;
  },

  /**
   * Gets the engine with a given alias.
   *
   * @param   {string} alias
   *          A search engine alias.
   * @returns {nsISearchEngine} The matching engine or null if there isn't one.
   */
  async engineForAlias(alias) {
    await this.ensureInitialized();

    return SearchAutocompleteProviderInternal
           .enginesByAlias.get(alias.toLocaleLowerCase()) || null;
  },

  /**
   * Gets the list of engines with token ("@") aliases.
   *
   * @returns {array<{ {nsISearchEngine} engine, {array<string>} tokenAliases }>}
   *          Array of objects { engine, tokenAliases } for token alias engines.
   */
  async tokenAliasEngines() {
    await this.ensureInitialized();

    return SearchAutocompleteProviderInternal.tokenAliasEngines.slice();
  },

  /**
   * Use this to get the current engine rather than Services.search.defaultEngine
   * directly.  This method makes sure that the service is first initialized.
   *
   * @returns {nsISearchEngine} The current search engine.
   */
  async currentEngine() {
    await this.ensureInitialized();

    return Services.search.defaultEngine;
  },

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
  parseSubmissionURL(url) {
    if (!SearchAutocompleteProviderInternal.initialized) {
      throw new Error("The component has not been initialized.");
    }

    let parseUrlResult = Services.search.parseSubmissionURL(url);
    return parseUrlResult.engine && {
      engineName: parseUrlResult.engine.name,
      terms: parseUrlResult.terms,
    };
  },

  /**
   * Starts a new suggestions fetch.
   *
   * @param   {nsISearchEngine} engine
   *          The engine from which suggestions will be fetched.
   * @param   {string} searchString
   *          The search query string.
   * @param   {bool} inPrivateContext
   *          Pass true if the fetch is being done in a private window.
   * @param   {int} maxLocalResults
   *          The maximum number of results to fetch from the user's local
   *          history.
   * @param   {int} maxRemoteResults
   *          The maximum number of results to fetch from the search engine.
   * @param   {int} userContextId
   *          The user context ID in which the fetch is being performed.
   * @returns {SuggestionsFetch} A new suggestions fetch object you should use
   *          to track the fetch.
   */
  newSuggestionsFetch(engine,
                      searchString,
                      inPrivateContext,
                      maxLocalResults,
                      maxRemoteResults,
                      userContextId) {
    if (!SearchAutocompleteProviderInternal.initialized) {
      throw new Error("The component has not been initialized.");
    }
    if (!engine) {
      throw new Error("`engine` is null");
    }
    return new SuggestionsFetch(engine, searchString, inPrivateContext,
                                maxLocalResults, maxRemoteResults,
                                userContextId);
  },
});
