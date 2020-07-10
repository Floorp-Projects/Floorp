/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FormAutoCompleteResult } = ChromeUtils.import(
  "resource://gre/modules/nsFormAutoCompleteResult.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "SearchSuggestionController",
  "resource://gre/modules/SearchSuggestionController.jsm"
);

/**
 * SuggestAutoComplete is a base class that implements nsIAutoCompleteSearch
 * and can collect results for a given search by using this._suggestionController.
 * We do it this way since the AutoCompleteController in Mozilla requires a
 * unique XPCOM Service for every search provider, even if the logic for two
 * providers is identical.
 * @constructor
 */
function SuggestAutoComplete() {
  this._init();
}
SuggestAutoComplete.prototype = {
  _init() {
    this._suggestionController = new SearchSuggestionController(obj =>
      this.onResultsReturned(obj)
    );
    this._suggestionController.maxLocalResults = this._historyLimit;
  },

  /**
   * The object implementing nsIAutoCompleteObserver that we notify when
   * we have found results
   * @private
   */
  _listener: null,

  /**
   * Maximum number of history items displayed. This is capped at 7
   * because the primary consumer (Firefox search bar) displays 10 rows
   * by default, and so we want to leave some space for suggestions
   * to be visible.
   */
  _historyLimit: 7,

  /**
   * Callback for handling results from SearchSuggestionController.jsm
   *
   * @param {Array} results
   *   The array of results that have been returned.
   * @private
   */
  onResultsReturned(results) {
    let finalResults = [];
    let finalComments = [];

    // If form history has results, add them to the list.
    for (let i = 0; i < results.local.length; ++i) {
      finalResults.push(results.local[i].value);
      finalComments.push("");
    }

    // If there are remote matches, add them.
    if (results.remote.length) {
      // "comments" column values for suggestions are empty strings
      let comments = new Array(results.remote.length).fill("");
      // now put the history results above the suggestions
      // We shouldn't show tail suggestions in their full-text form.
      let nonTailEntries = results.remote.filter(
        e => !e.matchPrefix && !e.tail
      );
      finalResults = finalResults.concat(nonTailEntries.map(e => e.value));
      finalComments = finalComments.concat(comments);
    }

    // Notify the FE of our new results
    this.onResultsReady(
      results.term,
      finalResults,
      finalComments,
      results.formHistoryResult
    );
  },

  /**
   * Notifies the front end of new results.
   *
   * @param {string} searchString
   *   The user's query string.
   * @param {array} results
   *   An array of results to the search.
   * @param {array} comments
   *   An array of metadata corresponding to the results.
   * @param {object} formHistoryResult
   *   Any previous form history result.
   * @private
   */
  onResultsReady(searchString, results, comments, formHistoryResult) {
    if (this._listener) {
      // Create a copy of the results array to use as labels, since
      // FormAutoCompleteResult doesn't like being passed the same array
      // for both.
      let labels = results.slice();
      let result = new FormAutoCompleteResult(
        searchString,
        Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
        0,
        "",
        results,
        labels,
        comments,
        formHistoryResult
      );

      this._listener.onSearchResult(this, result);

      // Null out listener to make sure we don't notify it twice
      this._listener = null;
    }
  },

  /**
   * Initiates the search result gathering process. Part of
   * nsIAutoCompleteSearch implementation.
   *
   * @param {string} searchString
   *   The user's query string.
   * @param {string} searchParam
   *   unused, "an extra parameter"; even though this parameter and the
   *   next are unused, pass them through in case the form history
   *   service wants them
   * @param {object} previousResult
   *   unused, a client-cached store of the previous generated resultset
   *   for faster searching.
   * @param {object} listener
   *   object implementing nsIAutoCompleteObserver which we notify when
   *   results are ready.
   */
  startSearch(searchString, searchParam, previousResult, listener) {
    // Don't reuse a previous form history result when it no longer applies.
    if (!previousResult) {
      this._formHistoryResult = null;
    }

    var formHistorySearchParam = searchParam.split("|")[0];

    // Receive the information about the privacy mode of the window to which
    // this search box belongs.  The front-end's search.xml bindings passes this
    // information in the searchParam parameter.  The alternative would have
    // been to modify nsIAutoCompleteSearch to add an argument to startSearch
    // and patch all of autocomplete to be aware of this, but the searchParam
    // argument is already an opaque argument, so this solution is hopefully
    // less hackish (although still gross.)
    var privacyMode = searchParam.split("|")[1] == "private";

    // Start search immediately if possible, otherwise once the search
    // service is initialized
    if (Services.search.isInitialized) {
      this._triggerSearch(
        searchString,
        formHistorySearchParam,
        listener,
        privacyMode
      );
      return;
    }

    Services.search
      .init()
      .then(() => {
        this._triggerSearch(
          searchString,
          formHistorySearchParam,
          listener,
          privacyMode
        );
      })
      .catch(result =>
        Cu.reportError(
          "Could not initialize search service, bailing out: " + result
        )
      );
  },

  /**
   * Actual implementation of search.
   *
   * @param {string} searchString
   *   The user's query string.
   * @param {string} searchParam
   *   unused
   * @param {object} listener
   *   object implementing nsIAutoCompleteObserver which we notify when
   *   results are ready.
   * @param {boolean} privacyMode
   *   True if the search was made from a private browsing mode context.
   */
  _triggerSearch(searchString, searchParam, listener, privacyMode) {
    this._listener = listener;
    this._suggestionController.fetch(
      searchString,
      privacyMode,
      Services.search.defaultEngine
    );
  },

  /**
   * Ends the search result gathering process. Part of nsIAutoCompleteSearch
   * implementation.
   */
  stopSearch() {
    this._suggestionController.stop();
  },

  // nsISupports
  QueryInterface: ChromeUtils.generateQI([
    "nsIAutoCompleteSearch",
    "nsIAutoCompleteObserver",
  ]),
};

/**
 * SearchSuggestAutoComplete is a service implementation that handles suggest
 * results specific to web searches.
 * @constructor
 */
function SearchSuggestAutoComplete() {
  // This calls _init() in the parent class (SuggestAutoComplete) via the
  // prototype, below.
  this._init();
}
SearchSuggestAutoComplete.prototype = {
  classID: Components.ID("{aa892eb4-ffbf-477d-9f9a-06c995ae9f27}"),
  __proto__: SuggestAutoComplete.prototype,
  serviceURL: "",
};

var EXPORTED_SYMBOLS = ["SearchSuggestAutoComplete"];
