/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SearchSuggestionController"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NS_ASSERT", "resource://gre/modules/debug.js");

const SEARCH_RESPONSE_SUGGESTION_JSON = "application/x-suggestions+json";
const DEFAULT_FORM_HISTORY_PARAM      = "searchbar-history";
const HTTP_OK            = 200;
const REMOTE_TIMEOUT     = 500; // maximum time (ms) to wait before giving up on a remote suggestions
const BROWSER_SUGGEST_PREF = "browser.search.suggest.enabled";

/**
 * Remote search suggestions will be shown if gRemoteSuggestionsEnabled
 * is true. Global because only one pref observer is needed for all instances.
 */
var gRemoteSuggestionsEnabled = Services.prefs.getBoolPref(BROWSER_SUGGEST_PREF);
Services.prefs.addObserver(BROWSER_SUGGEST_PREF, function(aSubject, aTopic, aData) {
  gRemoteSuggestionsEnabled = Services.prefs.getBoolPref(BROWSER_SUGGEST_PREF);
}, false);

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
this.SearchSuggestionController = function SearchSuggestionController(callback = null) {
  this._callback = callback;
};

this.SearchSuggestionController.prototype = {
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
   * The maximum time (ms) to wait before giving up on a remote suggestions.
   */
  remoteTimeout: REMOTE_TIMEOUT,

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
   * Fetch search suggestions from all of the providers. Fetches in progress will be stopped and
   * results from them will not be provided.
   *
   * @param {string} searchTerm - the term to provide suggestions for
   * @param {bool} privateMode - whether the request is being made in the context of private browsing
   * @param {nsISearchEngine} engine - search engine for the suggestions.
   *
   * @return {Promise} resolving to an object containing results or null.
   */
  fetch: function(searchTerm, privateMode, engine) {
    // There is no smart filtering from previous results here (as there is when looking through
    // history/form data) because the result set returned by the server is different for every typed
    // value - e.g. "ocean breathes" does not return a subset of the results returned for "ocean".

    this.stop();

    if (!Services.search.isInitialized) {
      throw new Error("Search not initialized yet (how did you get here?)");
    }
    if (typeof privateMode === "undefined") {
      throw new Error("The privateMode argument is required to avoid unintentional privacy leaks");
    }
    if (!(engine instanceof Ci.nsISearchEngine)) {
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
    if (searchTerm && gRemoteSuggestionsEnabled && this.maxRemoteResults &&
        engine.supportsResponseType(SEARCH_RESPONSE_SUGGESTION_JSON)) {
      this._deferredRemoteResult = this._fetchRemote(searchTerm, engine, privateMode);
      promises.push(this._deferredRemoteResult.promise);
    }

    // Local results from form history
    if (this.maxLocalResults) {
      let deferredHistoryResult = this._fetchFormHistory(searchTerm);
      promises.push(deferredHistoryResult.promise);
    }

    function handleRejection(reason) {
      if (reason == "HTTP request aborted") {
        // Do nothing since this is normal.
        return null;
      }
      Cu.reportError("SearchSuggestionController rejection: " + reason);
      return null;
    }
    return Promise.all(promises).then(this._dedupeAndReturnResults.bind(this), handleRejection);
  },

  /**
   * Stop pending fetches so no results are returned from them.
   *
   * Note: If there was no remote results fetched, the fetching cannot be stopped and local results
   * will still be returned because stopping relies on aborting the XMLHTTPRequest to reject the
   * promise for Promise.all.
   */
  stop: function() {
    if (this._request) {
      this._request.abort();
    } else if (!this.maxRemoteResults) {
      Cu.reportError("SearchSuggestionController: Cannot stop fetching if remote results were not "+
                     "requested");
    }
    this._reset();
  },

  // Private methods

  _fetchFormHistory: function(searchTerm) {
    let deferredFormHistory = Promise.defer();

    let acSearchObserver = {
      // Implements nsIAutoCompleteSearch
      onSearchResult: (search, result) => {
        this._formHistoryResult = result;

        if (this._request) {
          this._remoteResultTimer = Cc["@mozilla.org/timer;1"].
                                    createInstance(Ci.nsITimer);
          this._remoteResultTimer.initWithCallback(this._onRemoteTimeout.bind(this),
                                                   this.remoteTimeout || REMOTE_TIMEOUT,
                                                   Ci.nsITimer.TYPE_ONE_SHOT);
        }

        switch (result.searchResult) {
          case Ci.nsIAutoCompleteResult.RESULT_SUCCESS:
          case Ci.nsIAutoCompleteResult.RESULT_NOMATCH:
            if (result.searchString !== this._searchString) {
              deferredFormHistory.resolve("Unexpected response, this._searchString does not match form history response");
              return;
            }
            let fhEntries = [];
            for (let i = 0; i < result.matchCount; ++i) {
              fhEntries.push(result.getValueAt(i));
            }
            deferredFormHistory.resolve({
              result: fhEntries,
              formHistoryResult: result,
            });
            break;
          case Ci.nsIAutoCompleteResult.RESULT_FAILURE:
          case Ci.nsIAutoCompleteResult.RESULT_IGNORED:
            deferredFormHistory.resolve("Form History returned RESULT_FAILURE or RESULT_IGNORED");
            break;
        }
      },
    };

    let formHistory = Cc["@mozilla.org/autocomplete/search;1?name=form-history"].
                      createInstance(Ci.nsIAutoCompleteSearch);
    formHistory.startSearch(searchTerm, this.formHistoryParam || DEFAULT_FORM_HISTORY_PARAM,
                            this._formHistoryResult,
                            acSearchObserver);
    return deferredFormHistory;
  },

  /**
   * Fetch suggestions from the search engine over the network.
   */
  _fetchRemote: function(searchTerm, engine, privateMode) {
    let deferredResponse = Promise.defer();
    this._request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsIXMLHttpRequest);
    let submission = engine.getSubmission(searchTerm,
                                          SEARCH_RESPONSE_SUGGESTION_JSON);
    let method = (submission.postData ? "POST" : "GET");
    this._request.open(method, submission.uri.spec, true);
    if (this._request.channel instanceof Ci.nsIPrivateBrowsingChannel) {
      this._request.channel.setPrivate(privateMode);
    }
    this._request.mozBackgroundRequest = true; // suppress dialogs and fail silently

    this._request.addEventListener("load", this._onRemoteLoaded.bind(this, deferredResponse));
    this._request.addEventListener("error", (evt) => deferredResponse.resolve("HTTP error"));
    // Reject for an abort assuming it's always from .stop() in which case we shouldn't return local
    // or remote results for existing searches.
    this._request.addEventListener("abort", (evt) => deferredResponse.reject("HTTP request aborted"));

    this._request.send(submission.postData);

    return deferredResponse;
  },

  /**
   * Called when the request completed successfully (thought the HTTP status could be anything)
   * so we can handle the response data.
   * @private
   */
  _onRemoteLoaded: function(deferredResponse) {
    if (!this._request) {
      deferredResponse.resolve("Got HTTP response after the request was cancelled");
      return;
    }

    let status, serverResults;
    try {
      status = this._request.status;
    } catch (e) {
      // The XMLHttpRequest can throw NS_ERROR_NOT_AVAILABLE.
      deferredResponse.resolve("Unknown HTTP status: " + e);
      return;
    }

    if (status != HTTP_OK || this._request.responseText == "") {
      deferredResponse.resolve("Non-200 status or empty HTTP response: " + status);
      return;
    }

    try {
      serverResults = JSON.parse(this._request.responseText);
    } catch(ex) {
      deferredResponse.resolve("Failed to parse suggestion JSON: " + ex);
      return;
    }

    if (!serverResults[0] ||
        this._searchString.localeCompare(serverResults[0], undefined,
                                         { sensitivity: "base" })) {
      // something is wrong here so drop remote results
      deferredResponse.resolve("Unexpected response, this._searchString does not match remote response");
      return;
    }
    let results = serverResults[1] || [];
    deferredResponse.resolve({ result: results });
  },

  /**
   * Called when this._remoteResultTimer fires indicating the remote request took too long.
   */
  _onRemoteTimeout: function () {
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
   * @return {Object}
   */
  _dedupeAndReturnResults: function(suggestResults) {
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

    for (let result of suggestResults) {
      if (typeof result === "string") { // Failure message
        Cu.reportError("SearchSuggestionController: " + result);
      } else if (result.formHistoryResult) { // Local results have a formHistoryResult property.
        results.formHistoryResult = result.formHistoryResult;
        results.local = result.result || [];
      } else { // Remote result
        results.remote = result.result || [];
      }
    }

    // If we have remote results, cap the number of local results
    if (results.remote.length) {
      results.local = results.local.slice(0, this.maxLocalResults);
    }

    // We don't want things to appear in both history and suggestions so remove entries from
    // remote results that are already in local.
    if (results.remote.length && results.local.length) {
      for (let i = 0; i < results.local.length; ++i) {
        let term = results.local[i];
        let dupIndex = results.remote.indexOf(term);
        if (dupIndex != -1) {
          results.remote.splice(dupIndex, 1);
        }
      }
    }

    // Trim the number of results to the maximum requested (now that we've pruned dupes).
    results.remote =
      results.remote.slice(0, this.maxRemoteResults - results.local.length);

    if (this._callback) {
      this._callback(results);
    }
    this._reset();

    return results;
  },

  _reset: function() {
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
 * @return {boolean} True if the engine offers suggestions and false otherwise.
 */
this.SearchSuggestionController.engineOffersSuggestions = function(engine) {
 return engine.supportsResponseType(SEARCH_RESPONSE_SUGGESTION_JSON);
};
