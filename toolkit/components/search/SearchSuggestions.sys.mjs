/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FormAutoCompleteResult } = ChromeUtils.import(
  "resource://gre/modules/nsFormAutoCompleteResult.jsm"
);
const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  SearchSuggestionController:
    "resource://gre/modules/SearchSuggestionController.sys.mjs",
});

/**
 * SuggestAutoComplete is a base class that implements nsIAutoCompleteSearch
 * and can collect results for a given search by using this.#suggestionController.
 * We do it this way since the AutoCompleteController in Mozilla requires a
 * unique XPCOM Service for every search provider, even if the logic for two
 * providers is identical.
 * @constructor
 */
class SuggestAutoComplete {
  constructor() {
    this.#init();
  }

  /**
   * Callback for handling results from SearchSuggestionController.jsm
   *
   * @param {Array} results
   *   The array of results that have been returned.
   * @private
   */
  onResultsReturned(results) {
    let finalResults = [];

    // If form history has results, add them to the list.
    for (let i = 0; i < results.local.length; ++i) {
      finalResults.push(results.local[i].value);
    }

    // If there are remote matches, add them.
    if (results.remote.length) {
      // now put the history results above the suggestions
      // We shouldn't show tail suggestions in their full-text form.
      let nonTailEntries = results.remote.filter(
        e => !e.matchPrefix && !e.tail
      );
      finalResults = finalResults.concat(nonTailEntries.map(e => e.value));
    }

    // Notify the FE of our new results
    this.onResultsReady(results.term, finalResults, results.formHistoryResult);
  }

  /**
   * Notifies the front end of new results.
   *
   * @param {string} searchString
   *   The user's query string.
   * @param {array} results
   *   An array of results to the search.
   * @param {object} formHistoryResult
   *   Any previous form history result.
   * @private
   */
  onResultsReady(searchString, results, formHistoryResult) {
    if (this.#listener) {
      let result = new FormAutoCompleteResult(
        searchString,
        Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
        0,
        "",
        results.map(result => ({
          value: result,
          label: result,
          // We supply the comments field so that autocomplete does not kick
          // in the unescaping of the results for display which it uses for
          // urls.
          comment: result,
          removable: true,
        })),
        formHistoryResult
      );

      this.#listener.onSearchResult(this, result);

      // Null out listener to make sure we don't notify it twice
      this.#listener = null;
    }
  }

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
      this.#formHistoryResult = null;
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
      this.#triggerSearch(
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
        this.#triggerSearch(
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
  }

  /**
   * Ends the search result gathering process. Part of nsIAutoCompleteSearch
   * implementation.
   */
  stopSearch() {
    this.#suggestionController.stop();
  }

  #suggestionController;
  #formHistoryResult;

  /**
   * Maximum number of history items displayed. This is capped at 7
   * because the primary consumer (Firefox search bar) displays 10 rows
   * by default, and so we want to leave some space for suggestions
   * to be visible.
   *
   * @type {number}
   */
  #historyLimit = 7;

  /**
   * The object implementing nsIAutoCompleteObserver that we notify when
   * we have found results.
   *
   *  @type {object|null}
   */
  #listener = null;

  #init() {
    this.#suggestionController = new lazy.SearchSuggestionController(obj =>
      this.onResultsReturned(obj)
    );
    this.#suggestionController.maxLocalResults = this.#historyLimit;
  }

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
  #triggerSearch(searchString, searchParam, listener, privacyMode) {
    this.#listener = listener;
    this.#suggestionController.fetch(
      searchString,
      privacyMode,
      Services.search.defaultEngine
    );
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsIAutoCompleteSearch",
    "nsIAutoCompleteObserver",
  ]);
}

/**
 * SearchSuggestAutoComplete is a service implementation that handles suggest
 * results specific to web searches.
 * @constructor
 */
export class SearchSuggestAutoComplete extends SuggestAutoComplete {
  classID = Components.ID("{aa892eb4-ffbf-477d-9f9a-06c995ae9f27}");
  serviceURL = "";
}
