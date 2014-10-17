/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/nsFormAutoCompleteResult.jsm");
Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SearchSuggestionController",
                                  "resource://gre/modules/SearchSuggestionController.jsm");

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

  _init: function() {
    this._suggestionController = new SearchSuggestionController(obj => this.onResultsReturned(obj));
    this._suggestionController.maxLocalResults = this._historyLimit;
  },

  get _suggestionLabel() {
    delete this._suggestionLabel;
    let bundle = Services.strings.createBundle("chrome://global/locale/search/search.properties");
    return this._suggestionLabel = bundle.GetStringFromName("suggestion_label");
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
   * @private
   */
  onResultsReturned: function(results) {
    let finalResults = [];
    let finalComments = [];

    // If form history has results, add them to the list.
    for (let i = 0; i < results.local.length; ++i) {
      finalResults.push(results.local[i]);
      finalComments.push("");
    }

    // If there are remote matches, add them.
    if (results.remote.length) {
      // "comments" column values for suggestions starts as empty strings
      let comments = new Array(results.remote.length).fill("", 1);
      comments[0] = this._suggestionLabel;
      // now put the history results above the suggestions
      finalResults = finalResults.concat(results.remote);
      finalComments = finalComments.concat(comments);
    }

    // Notify the FE of our new results
    this.onResultsReady(results.term, finalResults, finalComments, results.formHistoryResult);
  },

  /**
   * Notifies the front end of new results.
   * @param searchString  the user's query string
   * @param results       an array of results to the search
   * @param comments      an array of metadata corresponding to the results
   * @private
   */
  onResultsReady: function(searchString, results, comments, formHistoryResult) {
    if (this._listener) {
      let result = new FormAutoCompleteResult(
          searchString,
          Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
          0,
          "",
          results,
          results,
          comments,
          formHistoryResult);

      this._listener.onSearchResult(this, result);

      // Null out listener to make sure we don't notify it twice
      this._listener = null;
    }
  },

  /**
   * Initiates the search result gathering process. Part of
   * nsIAutoCompleteSearch implementation.
   *
   * @param searchString    the user's query string
   * @param searchParam     unused, "an extra parameter"; even though
   *                        this parameter and the next are unused, pass
   *                        them through in case the form history
   *                        service wants them
   * @param previousResult  unused, a client-cached store of the previous
   *                        generated resultset for faster searching.
   * @param listener        object implementing nsIAutoCompleteObserver which
   *                        we notify when results are ready.
   */
  startSearch: function(searchString, searchParam, previousResult, listener) {
    // Don't reuse a previous form history result when it no longer applies.
    if (!previousResult)
      this._formHistoryResult = null;

    var formHistorySearchParam = searchParam.split("|")[0];

    // Receive the information about the privacy mode of the window to which
    // this search box belongs.  The front-end's search.xml bindings passes this
    // information in the searchParam parameter.  The alternative would have
    // been to modify nsIAutoCompleteSearch to add an argument to startSearch
    // and patch all of autocomplete to be aware of this, but the searchParam
    // argument is already an opaque argument, so this solution is hopefully
    // less hackish (although still gross.)
    var privacyMode = (searchParam.split("|")[1] == "private");

    // Start search immediately if possible, otherwise once the search
    // service is initialized
    if (Services.search.isInitialized) {
      this._triggerSearch(searchString, formHistorySearchParam, listener, privacyMode);
      return;
    }

    Services.search.init((function startSearch_cb(aResult) {
      if (!Components.isSuccessCode(aResult)) {
        Cu.reportError("Could not initialize search service, bailing out: " + aResult);
        return;
      }
      this._triggerSearch(searchString, formHistorySearchParam, listener, privacyMode);
    }).bind(this));
  },

  /**
   * Actual implementation of search.
   */
  _triggerSearch: function(searchString, searchParam, listener, privacyMode) {
    this._listener = listener;
    this._suggestionController.fetch(searchString,
                                     privacyMode,
                                     Services.search.currentEngine);
  },

  /**
   * Ends the search result gathering process. Part of nsIAutoCompleteSearch
   * implementation.
   */
  stopSearch: function() {
    this._suggestionController.stop();
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteSearch,
                                         Ci.nsIAutoCompleteObserver])
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
  serviceURL: ""
};

var component = [SearchSuggestAutoComplete];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
