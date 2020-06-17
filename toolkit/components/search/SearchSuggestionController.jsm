/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SearchSuggestionController"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const DEFAULT_FORM_HISTORY_PARAM = "searchbar-history";
const HTTP_OK = 200;
const BROWSER_SUGGEST_PREF = "browser.search.suggest.enabled";
const BROWSER_SUGGEST_PRIVATE_PREF = "browser.search.suggest.enabled.private";
const REMOTE_TIMEOUT_PREF = "browser.search.suggest.timeout";
const REMOTE_TIMEOUT_DEFAULT = 500; // maximum time (ms) to wait before giving up on a remote suggestions

const SEARCH_DATA_TRANSFERRED_SCALAR = "browser.search.data_transferred";
const SEARCH_TELEMETRY_KEY_PREFIX = "sggt";
const SEARCH_TELEMETRY_PRIVATE_BROWSING_KEY_SUFFIX = "pb";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "UUIDGenerator",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

/**
 * Generates an UUID.
 *
 * @returns {string}
 *   An UUID string, without leading or trailing braces.
 */
function uuid() {
  let uuid = UUIDGenerator.generateUUID().toString();
  return uuid.slice(1, uuid.length - 1);
}

/**
 * Represents a search suggestion.
 * TODO: Support other Google tail fields: `a`, `dc`, `i`, `q`, `ansa`,
 * `ansb`, `ansc`, `du`. See bug 1626897 comment 2.
 */
class SearchSuggestionEntry {
  /**
   * Creates an entry.
   * @param {string} value
   *   The suggestion as a full-text string. Suitable for display directly to
   *   the user.
   * @param {string} [matchPrefix]
   *   Represents the part of a tail suggestion that is already typed. For
   *   example, Google returns "…" as the match prefix to replace
   *   "what time is it in" in a tail suggestion for the query
   *   "what time is it in t".
   * @param {string} [tail]
   *   Represents the suggested part of a tail suggestion. For example, Google
   *   might return "toronto" as the tail for the query "what time is it in t".
   */
  constructor(value, { matchPrefix, tail } = {}) {
    this._value = value;
    this._matchPrefix = matchPrefix;
    this._tail = tail;
  }

  /**
   * Returns true if `otherEntry` is equivalent to this instance of
   * SearchSuggestionEntry.
   * @param {SearchSuggestionEntry} otherEntry
   * @returns {boolean}
   */
  equals(otherEntry) {
    return otherEntry.value == this.value;
  }

  get value() {
    return this._value;
  }

  get matchPrefix() {
    return this._matchPrefix;
  }

  get tail() {
    return this._tail;
  }

  get tailOffsetIndex() {
    if (!this._tail) {
      return -1;
    }

    let offsetIndex = this._value.lastIndexOf(this._tail);
    if (offsetIndex + this._tail.length < this._value.length) {
      // We might have a tail suggestion that starts with a word contained in
      // the full-text suggestion. e.g. "london sights in l" ... "london".
      let lastWordIndex = this._value.lastIndexOf(" ");
      if (this._tail.startsWith(this._value.substring(lastWordIndex))) {
        offsetIndex = lastWordIndex;
      } else {
        // Something's gone wrong. Consumers should not show this result.
        offsetIndex = -1;
      }
    }

    return offsetIndex;
  }
}

// Maps each engine name to a unique firstPartyDomain, so that requests to
// different engines are isolated from each other and from normal browsing.
// This is the same for all the controllers.
var gFirstPartyDomains = new Map();

/**
 * SearchSuggestionController.jsm exists as a helper module to allow multiple consumers to request and display
 * search suggestions from a given engine, regardless of the base implementation. Much of this
 * code was originally in nsSearchSuggestions.js until it was refactored to separate it from the
 * nsIAutoCompleteSearch dependency.
 * One instance of SearchSuggestionController should be used per field since form history results are cached.
 */

/**
 * @param {function} [callback] - Callback for search suggestion results. You can use the promise
 *                                returned by the search method instead if you prefer.
 * @constructor
 */
function SearchSuggestionController(callback = null) {
  this._callback = callback;
}

SearchSuggestionController.prototype = {
  /**
   * The maximum number of local form history results to return. This limit is
   * only enforced if remote results are also returned.
   */
  maxLocalResults: 5,

  /**
   * The maximum number of remote search engine results to return.
   * We'll actually only display at most
   * maxRemoteResults - <displayed local results count> remote results.
   */
  maxRemoteResults: 10,

  /**
   * The additional parameter used when searching form history.
   */
  formHistoryParam: DEFAULT_FORM_HISTORY_PARAM,

  // Private properties
  /**
   * The last form history result used to improve the performance of subsequent searches.
   * This shouldn't be used for any other purpose as it is never cleared and therefore could be stale.
   */
  _formHistoryResult: null,

  /**
   * The remote server timeout timer, if applicable. The timer starts when form history
   * search is completed.
   */
  _remoteResultTimer: null,

  /**
   * The deferred for the remote results before its promise is resolved.
   */
  _deferredRemoteResult: null,

  /**
   * The optional result callback registered from the constructor.
   */
  _callback: null,

  /**
   * The XMLHttpRequest object for remote results.
   */
  _request: null,

  // Public methods

  /**
   * Gets the firstPartyDomains Map, useful for tests.
   * @returns {Map} firstPartyDomains mapped by engine names.
   */
  get firstPartyDomains() {
    return gFirstPartyDomains;
  },

  /**
   * Fetch search suggestions from all of the providers. Fetches in progress will be stopped and
   * results from them will not be provided.
   *
   * @param {string} searchTerm - the term to provide suggestions for
   * @param {boolean} privateMode - whether the request is being made in the context of private browsing
   * @param {nsISearchEngine} engine - search engine for the suggestions.
   * @param {int} userContextId - the userContextId of the selected tab.
   *
   * @returns {Promise} resolving to an object with the following contents:
   * @returns {array<SearchSuggestionEntry>} results.local
   *   Contains local search suggestions.
   * @returns {array<SearchSuggestionEntry>} results.remote
   *   Contains remote search suggestions.
   */
  fetch(searchTerm, privateMode, engine, userContextId = 0) {
    // There is no smart filtering from previous results here (as there is when looking through
    // history/form data) because the result set returned by the server is different for every typed
    // value - e.g. "ocean breathes" does not return a subset of the results returned for "ocean".

    this.stop();

    if (!Services.search.isInitialized) {
      throw new Error("Search not initialized yet (how did you get here?)");
    }
    if (typeof privateMode === "undefined") {
      throw new Error(
        "The privateMode argument is required to avoid unintentional privacy leaks"
      );
    }
    if (!engine.getSubmission) {
      throw new Error("Invalid search engine");
    }
    if (!this.maxLocalResults && !this.maxRemoteResults) {
      throw new Error("Zero results expected, what are you trying to do?");
    }
    if (this.maxLocalResults < 0 || this.maxRemoteResults < 0) {
      throw new Error("Number of requested results must be positive");
    }

    // Array of promises to resolve before returning results.
    let promises = [];
    this._searchString = searchTerm;

    // Remote results
    if (
      searchTerm &&
      this.suggestionsEnabled &&
      (!privateMode || this.suggestionsInPrivateBrowsingEnabled) &&
      this.maxRemoteResults &&
      engine.supportsResponseType(SearchUtils.URL_TYPE.SUGGEST_JSON)
    ) {
      this._deferredRemoteResult = this._fetchRemote(
        searchTerm,
        engine,
        privateMode,
        userContextId
      );
      promises.push(this._deferredRemoteResult.promise);
    }

    // Local results from form history
    if (this.maxLocalResults) {
      promises.push(this._fetchFormHistory(searchTerm));
    }

    function handleRejection(reason) {
      if (reason == "HTTP request aborted") {
        // Do nothing since this is normal.
        return null;
      }
      Cu.reportError("SearchSuggestionController rejection: " + reason);
      return null;
    }
    return Promise.all(promises).then(
      this._dedupeAndReturnResults.bind(this),
      handleRejection
    );
  },

  /**
   * Stop pending fetches so no results are returned from them.
   *
   * Note: If there was no remote results fetched, the fetching cannot be stopped and local results
   * will still be returned because stopping relies on aborting the XMLHTTPRequest to reject the
   * promise for Promise.all.
   */
  stop() {
    if (this._request) {
      this._request.abort();
    }
    this._reset();
  },

  // Private methods

  _fetchFormHistory(searchTerm) {
    return new Promise(resolve => {
      let acSearchObserver = {
        // Implements nsIAutoCompleteSearch
        onSearchResult: (search, result) => {
          this._formHistoryResult = result;

          if (this._request) {
            this._remoteResultTimer = Cc["@mozilla.org/timer;1"].createInstance(
              Ci.nsITimer
            );
            this._remoteResultTimer.initWithCallback(
              this._onRemoteTimeout.bind(this),
              this.remoteTimeout,
              Ci.nsITimer.TYPE_ONE_SHOT
            );
          }

          switch (result.searchResult) {
            case Ci.nsIAutoCompleteResult.RESULT_SUCCESS:
            case Ci.nsIAutoCompleteResult.RESULT_NOMATCH:
              if (result.searchString !== this._searchString) {
                resolve(
                  "Unexpected response, this._searchString does not match form history response"
                );
                return;
              }
              let fhEntries = [];
              for (let i = 0; i < result.matchCount; ++i) {
                fhEntries.push(result.getValueAt(i));
              }
              resolve({
                result: fhEntries,
                formHistoryResult: result,
              });
              break;
            case Ci.nsIAutoCompleteResult.RESULT_FAILURE:
            case Ci.nsIAutoCompleteResult.RESULT_IGNORED:
              resolve("Form History returned RESULT_FAILURE or RESULT_IGNORED");
              break;
          }
        },
      };

      let formHistory = Cc[
        "@mozilla.org/autocomplete/search;1?name=form-history"
      ].createInstance(Ci.nsIAutoCompleteSearch);
      formHistory.startSearch(
        searchTerm,
        this.formHistoryParam || DEFAULT_FORM_HISTORY_PARAM,
        this._formHistoryResult,
        acSearchObserver
      );
    });
  },

  /**
   * Report bandwidth used by search activities. It only reports when it matches
   * search provider information.
   *
   * @param {string} engineId the name of the search provider.
   * @param {boolean} privateMode set to true if this is coming from a private browsing mode request.
   */
  _reportBandwidthForEngine(engineId, privateMode) {
    if (!this._request || !this._request.channel) {
      return;
    }

    let channel = ChannelWrapper.get(this._request.channel);
    let bytesTransferred = channel.requestSize + channel.responseSize;
    if (bytesTransferred == 0) {
      return;
    }

    let telemetryKey = `${SEARCH_TELEMETRY_KEY_PREFIX}-${engineId}`;
    if (privateMode) {
      telemetryKey += `-${SEARCH_TELEMETRY_PRIVATE_BROWSING_KEY_SUFFIX}`;
    }

    Services.telemetry.keyedScalarAdd(
      SEARCH_DATA_TRANSFERRED_SCALAR,
      telemetryKey,
      bytesTransferred
    );
  },

  /**
   * Fetch suggestions from the search engine over the network.
   *
   * @param {string} searchTerm
   *   The term being searched for.
   * @param {SearchEngine} engine
   *   The engine to request suggestions from.
   * @param {boolean} privateMode
   *   Set to true if this is coming from a private browsing mode request.
   * @param {number} userContextId
   *   The id of the user container this request was made from.
   * @returns {Promise}
   *   Returns a promise that is resolved when the response is received, or
   *   rejected if there is an error.
   */
  _fetchRemote(searchTerm, engine, privateMode, userContextId) {
    let deferredResponse = PromiseUtils.defer();
    this._request = new XMLHttpRequest();
    let submission = engine.getSubmission(
      searchTerm,
      SearchUtils.URL_TYPE.SUGGEST_JSON
    );
    let method = submission.postData ? "POST" : "GET";
    this._request.open(method, submission.uri.spec, true);
    // Don't set or store cookies or on-disk cache.
    this._request.channel.loadFlags =
      Ci.nsIChannel.LOAD_ANONYMOUS | Ci.nsIChannel.INHIBIT_PERSISTENT_CACHING;
    // Use a unique first-party domain for each engine, to isolate the
    // suggestions requests.
    if (!gFirstPartyDomains.has(engine.name)) {
      // Use the engine identifier, or an uuid when not available, because the
      // domain cannot contain invalid chars and the engine name may not be
      // suitable. When using an uuid the firstPartyDomain of the same engine
      // will differ across restarts, but that's acceptable for now.
      // TODO (Bug 1511339): use a persistent unique identifier per engine.
      gFirstPartyDomains.set(
        engine.name,
        `${engine.identifier || uuid()}.search.suggestions.mozilla`
      );
    }
    let firstPartyDomain = gFirstPartyDomains.get(engine.name);

    this._request.setOriginAttributes({
      userContextId,
      privateBrowsingId: privateMode ? 1 : 0,
      firstPartyDomain,
    });

    this._request.mozBackgroundRequest = true; // suppress dialogs and fail silently

    let engineId = engine.identifier || "other";

    this._request.addEventListener(
      "load",
      this._onRemoteLoaded.bind(this, deferredResponse, engineId, privateMode)
    );
    this._request.addEventListener("error", evt => {
      this._reportBandwidthForEngine(engineId, privateMode);
      deferredResponse.resolve("HTTP error");
    });
    // Reject for an abort assuming it's always from .stop() in which case we shouldn't return local
    // or remote results for existing searches.
    this._request.addEventListener("abort", evt => {
      this._reportBandwidthForEngine(engineId, privateMode);
      deferredResponse.reject("HTTP request aborted");
    });

    if (submission.postData) {
      this._request.sendInputStream(submission.postData);
    } else {
      this._request.send();
    }

    return deferredResponse;
  },

  /**
   * Called when the request completed successfully (thought the HTTP status could be anything)
   * so we can handle the response data.
   *
   * @param {Promise} deferredResponse
   *   The promise to resolve when a response is received.
   * @param {string} engineId
   *   The name of the search provider.
   * @param {boolean} privateMode
   *   Set to true if this is coming from a private browsing mode request.
   * @private
   */
  _onRemoteLoaded(deferredResponse, engineId, privateMode) {
    if (!this._request) {
      deferredResponse.resolve(
        "Got HTTP response after the request was cancelled"
      );
      return;
    }

    this._reportBandwidthForEngine(engineId, privateMode);

    let status, serverResults;
    try {
      status = this._request.status;
    } catch (e) {
      // The XMLHttpRequest can throw NS_ERROR_NOT_AVAILABLE.
      deferredResponse.resolve("Unknown HTTP status: " + e);
      return;
    }

    if (status != HTTP_OK || this._request.responseText == "") {
      deferredResponse.resolve(
        "Non-200 status or empty HTTP response: " + status
      );
      return;
    }

    try {
      serverResults = JSON.parse(this._request.responseText);
    } catch (ex) {
      deferredResponse.resolve("Failed to parse suggestion JSON: " + ex);
      return;
    }

    if (
      !Array.isArray(serverResults) ||
      !serverResults[0] ||
      this._searchString.localeCompare(serverResults[0], undefined, {
        sensitivity: "base",
      })
    ) {
      // something is wrong here so drop remote results
      deferredResponse.resolve(
        "Unexpected response, this._searchString does not match remote response"
      );
      return;
    }

    // Remove the search string from the server results since it is no longer
    // needed.
    let results = serverResults.slice(1) || [];
    deferredResponse.resolve({ result: results });
  },

  /**
   * Called when this._remoteResultTimer fires indicating the remote request took too long.
   */
  _onRemoteTimeout() {
    this._request = null;

    // FIXME: bug 387341
    // Need to break the cycle between us and the timer.
    this._remoteResultTimer = null;

    // The XMLHTTPRequest for suggest results is taking too long
    // so send out the form history results and cancel the request.
    if (this._deferredRemoteResult) {
      this._deferredRemoteResult.resolve("HTTP Timeout");
      this._deferredRemoteResult = null;
    }
  },

  /**
   * @param {Array} suggestResults - an array of result objects from different sources (local or remote)
   * @returns {object}
   */
  _dedupeAndReturnResults(suggestResults) {
    if (this._searchString === null) {
      // _searchString can be null if stop() was called and remote suggestions
      // were disabled (stopping if we are fetching remote suggestions will
      // cause a promise rejection before we reach _dedupeAndReturnResults).
      return null;
    }

    let results = {
      term: this._searchString,
      remote: [],
      local: [],
      formHistoryResult: null,
    };

    for (let resultData of suggestResults) {
      if (typeof result === "string") {
        // Failure message
        Cu.reportError(
          "SearchSuggestionController found an unexpected string value: " +
            resultData
        );
      } else if (resultData.formHistoryResult) {
        // Local results have a formHistoryResult property.
        results.formHistoryResult = resultData.formHistoryResult;
        if (resultData.result) {
          results.local = resultData.result.map(
            s => new SearchSuggestionEntry(s)
          );
        }
      } else if (resultData.result) {
        // Remote result
        let richSuggestionData = this._getRichSuggestionData(resultData.result);
        let fullTextSuggestions = resultData.result[0];
        for (let i = 0; i < fullTextSuggestions.length; ++i) {
          results.remote.push(
            this._newSearchSuggestionEntry(
              fullTextSuggestions[i],
              richSuggestionData?.[i]
            )
          );
        }
      }
    }

    // If we have remote results, cap the number of local results
    if (results.remote.length) {
      results.local = results.local.slice(0, this.maxLocalResults);
    }

    // We don't want things to appear in both history and suggestions so remove
    // entries from remote results that are already in local.
    if (results.remote.length && results.local.length) {
      for (let i = 0; i < results.local.length; ++i) {
        let dupIndex = results.remote.findIndex(e =>
          e.equals(results.local[i])
        );
        if (dupIndex != -1) {
          results.remote.splice(dupIndex, 1);
        }
      }
    }

    // Trim the number of results to the maximum requested (now that we've pruned dupes).
    results.remote = results.remote.slice(
      0,
      this.maxRemoteResults - results.local.length
    );

    if (this._callback) {
      this._callback(results);
    }
    this._reset();

    return results;
  },

  /**
   * Returns rich suggestion data from a remote fetch, if available.
   * @param {array} remoteResultData
   *  The results.remote array returned by SearchSuggestionsController.fetch.
   * @returns {array}
   *  An array of additional rich suggestion data. Each element should
   *  correspond to the array of text suggestions.
   */
  _getRichSuggestionData(remoteResultData) {
    if (!remoteResultData || !Array.isArray(remoteResultData)) {
      return undefined;
    }

    for (let entry of remoteResultData) {
      if (
        typeof entry == "object" &&
        entry.hasOwnProperty("google:suggestdetail")
      ) {
        let richData = entry["google:suggestdetail"];
        if (
          Array.isArray(richData) &&
          richData.length == remoteResultData[0].length
        ) {
          return richData;
        }
      }
    }
    return undefined;
  },

  /**
   * Given a text suggestion and rich suggestion data, returns a
   * SearchSuggestionEntry.
   * @param {string} suggestion
   *   A suggestion string.
   * @param {object} richSuggestionData
   *   Rich suggestion data returned by the engine. In Google's case, this is
   *   the corresponding entry at "google:suggestdetail".
   * @returns {SearchSuggestionEntry}
   */
  _newSearchSuggestionEntry(suggestion, richSuggestionData) {
    if (richSuggestionData) {
      // We have valid rich suggestions.
      return new SearchSuggestionEntry(suggestion, {
        matchPrefix: richSuggestionData?.mp,
        tail: richSuggestionData?.t,
      });
    }
    // Return a regular suggestion.
    return new SearchSuggestionEntry(suggestion);
  },

  _reset() {
    this._request = null;
    if (this._remoteResultTimer) {
      this._remoteResultTimer.cancel();
      this._remoteResultTimer = null;
    }
    this._deferredRemoteResult = null;
    this._searchString = null;
  },
};

/**
 * Determines whether the given engine offers search suggestions.
 *
 * @param {nsISearchEngine} engine - The search engine
 * @returns {boolean} True if the engine offers suggestions and false otherwise.
 */
SearchSuggestionController.engineOffersSuggestions = function(engine) {
  return engine.supportsResponseType(SearchUtils.URL_TYPE.SUGGEST_JSON);
};

/**
 * The maximum time (ms) to wait before giving up on a remote suggestions.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  SearchSuggestionController.prototype,
  "remoteTimeout",
  REMOTE_TIMEOUT_PREF,
  REMOTE_TIMEOUT_DEFAULT
);

/**
 * Whether or not remote suggestions are turned on.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  SearchSuggestionController.prototype,
  "suggestionsEnabled",
  BROWSER_SUGGEST_PREF,
  true
);

/**
 * Whether or not remote suggestions are turned on in private browsing mode.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  SearchSuggestionController.prototype,
  "suggestionsInPrivateBrowsingEnabled",
  BROWSER_SUGGEST_PRIVATE_PREF,
  false
);
