/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  SearchSuggestionController:
    "resource://gre/modules/SearchSuggestionController.sys.mjs",
});

/**
 * A search history autocomplete result that implements nsIAutoCompleteResult.
 * Based on FormHistoryAutoCompleteResult.
 */
class SearchHistoryResult {
  /**
   * The name of the associated field in form history.
   *
   * @type {string}
   */
  #formFieldName = null;

  /**
   * An array of entries from form history.
   *
   * @type {object[]|null}
   */
  #formHistoryEntries = null;

  //
  // An array of entries that have come from a remote source and cannot
  // be deleted. These are listed after the form history entries.
  //
  // @type {object[]}
  // (using proper JSDoc comment here causes sphinx-js failures:
  //  https://github.com/mozilla/sphinx-js/issues/242).
  //
  #remoteEntries = [];

  QueryInterface = ChromeUtils.generateQI([
    "nsIAutoCompleteResult",
    "nsISupportsWeakReference",
  ]);

  /**
   * Constructor
   *
   * @param {string} formFieldName
   *   The name of the associated field in form history.
   * @param {string} searchString
   *   The search string used for the search.
   * @param {object[]} formHistoryEntries
   *   The entries received from form history.
   */
  constructor(formFieldName, searchString, formHistoryEntries) {
    this.#formFieldName = formFieldName;
    this.searchString = searchString;
    this.#formHistoryEntries = formHistoryEntries;
  }

  /**
   * Sets the remote entries and de-dupes them against the form history entries.
   *
   * @param {object[]} remoteEntries
   *   The fixed entries to save.
   */
  set remoteEntries(remoteEntries) {
    this.#remoteEntries = remoteEntries;
    this.#removeDuplicateHistoryEntries();
  }

  /**
   * The search string associated with this result.
   *
   * @type {string}
   */
  searchString = "";

  /**
   * An error description, always blank for these results.
   *
   * @type {string}
   */
  errorDescription = "";

  /**
   * Index of the default item that should be entered if none is selected.
   *
   * @returns {number}
   */
  get defaultIndex() {
    return this.matchCount ? 0 : -1;
  }

  /**
   * The result of the search.
   *
   * @returns {number}
   */
  get searchResult() {
    return this.matchCount
      ? Ci.nsIAutoCompleteResult.RESULT_SUCCESS
      : Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
  }

  /**
   * The number of matches.
   *
   * @returns {number}
   */
  get matchCount() {
    return this.#formHistoryEntries.length + this.#remoteEntries.length;
  }

  /**
   * Gets the value of the result at the given index. This is the value that
   * will be filled into the text field.
   *
   * @param {number} index
   *   The index of the result.
   * @returns {string}
   */
  getValueAt(index) {
    const item = this.#getAt(index);
    return item.text || item.value;
  }

  /**
   * Gets the label at the given index. This is the string that is displayed
   * in the autocomplete dropdown row. If there is additional text to be
   * displayed, it should be stored within a field in the comment.
   *
   * @param {number} index
   *   The index of the result.
   * @returns {string}
   */
  getLabelAt(index) {
    const item = this.#getAt(index);
    return item.text || item.label || item.value;
  }

  /**
   * Get the comment of the result at the given index. This is a serialized
   * JSON object containing additional properties related to the index.
   *
   * @param {number} index
   *   The index of the result.
   * @returns {string}
   */
  getCommentAt(index) {
    return this.#getAt(index).comment ?? "";
  }

  /**
   * Gets the style hint for the result at the given index.
   *
   * @param {number} index
   *   The index of the result.
   * @returns {string}
   */
  getStyleAt(index) {
    const itemStyle = this.#getAt(index).style;
    if (itemStyle) {
      return itemStyle;
    }

    if (index >= 0) {
      if (index < this.#formHistoryEntries.length) {
        return "fromhistory";
      }

      if (index > 0 && index == this.#formHistoryEntries.length) {
        return "datalist-first";
      }
    }
    return "";
  }

  /**
   * Gets the image of the result at the given index.
   *
   * @param {number} _index
   *   The index of the result.
   * @returns {string}
   */
  getImageAt(_index) {
    return "";
  }

  /**
   * Gets the final value that should be completed when the user confirms
   * the match at the given index.
   *
   * @param {number} index
   *   The index of the result.
   * @returns {string}
   */
  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  }

  /**
   * True if the value at the given index is removable.
   *
   * @param {number} index
   *   The index of the result.
   * @returns {boolean}
   */
  isRemovableAt(index) {
    return this.#isFormHistoryEntry(index);
  }

  /**
   * Remove the value at the given index from the autocomplete results.
   *
   * @param {number} index
   *   The index of the result.
   */
  removeValueAt(index) {
    if (this.isRemovableAt(index)) {
      const [removedEntry] = this.#formHistoryEntries.splice(index, 1);
      lazy.FormHistory.update({
        op: "remove",
        fieldname: this.#formFieldName,
        value: removedEntry.text,
        guid: removedEntry.guid,
      });
    }
  }

  /**
   * Returns the entry at the given index taking into account both the
   * form history entries and the remote entries.
   *
   * @param {number} index
   *   The index of the entry to find.
   * @returns {object}
   *   The object at the given index.
   * @throws {Components.Exception}
   *   Throws if the index is out of range.
   */
  #getAt(index) {
    for (const group of [this.#formHistoryEntries, this.#remoteEntries]) {
      if (index < group.length) {
        return group[index];
      }
      index -= group.length;
    }

    throw Components.Exception(
      "Index out of range.",
      Cr.NS_ERROR_ILLEGAL_VALUE
    );
  }

  /**
   * Returns true if the value at the given index is one of the form history
   * entries.
   *
   * @param {number} index
   *   The index of the result.
   * @returns {boolean}
   */

  #isFormHistoryEntry(index) {
    return index >= 0 && index < this.#formHistoryEntries.length;
  }

  /**
   * Remove items from history list that are already present in fixed list.
   * We do this rather than the opposite ( i.e. remove items from fixed list)
   * to reflect the order that is specified in the fixed list.
   */
  #removeDuplicateHistoryEntries() {
    this.#formHistoryEntries = this.#formHistoryEntries.filter(entry =>
      this.#remoteEntries.every(
        fixed => entry.text != (fixed.label || fixed.value)
      )
    );
  }
}

/**
 * SuggestAutoComplete is a base class that implements nsIAutoCompleteSearch
 * and can collect results for a given search by using this.#suggestionController.
 * We do it this way since the AutoCompleteController in Mozilla requires a
 * unique XPCOM Service for every search provider, even if the logic for two
 * providers is identical.
 *
 * @class
 */
class SuggestAutoComplete {
  constructor() {
    this.#suggestionController = new lazy.SearchSuggestionController();
    this.#suggestionController.maxLocalResults = this.#historyLimit;
  }

  /**
   * Notifies the front end of new results.
   *
   * @param {FormHistoryAutoCompleteResult} result
   *   Any previous form history result.
   * @private
   */
  onResultsReady(result) {
    if (this.#listener) {
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
      ).catch(console.error);
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
        ).catch(console.error);
      })
      .catch(result =>
        console.error(
          "Could not initialize search service, bailing out:",
          result
        )
      );
  }

  /**
   * Ends the search result gathering process. Part of nsIAutoCompleteSearch
   * implementation.
   */
  stopSearch() {
    // Prevent reporting results of stopped search
    this.#listener = null;
    this.#suggestionController.stop();
  }

  #suggestionController;

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
  async #triggerSearch(searchString, searchParam, listener, privacyMode) {
    this.#listener = listener;
    let results = await this.#suggestionController.fetch(
      searchString,
      privacyMode,
      Services.search.defaultEngine
    );

    let formHistoryEntries = (results?.formHistoryResults ?? []).map(
      historyEntry => ({
        // We supply the comments field so that autocomplete does not kick
        // in the unescaping of the results for display which it uses for
        // urls.
        comment: historyEntry.text,
        ...historyEntry,
      })
    );
    let autoCompleteResult = new SearchHistoryResult(
      this.#suggestionController.formHistoryParam,
      searchString,
      formHistoryEntries
    );

    if (results?.remote?.length) {
      // We shouldn't show tail suggestions in their full-text form.
      // Suggestions are shown after form history results.
      autoCompleteResult.remoteEntries = results.remote.reduce(
        (results, item) => {
          if (!item.matchPrefix && !item.tail) {
            results.push({
              value: item.value,
              label: item.value,
              // We supply the comments field so that autocomplete does not kick
              // in the unescaping of the results for display which it uses for
              // urls.
              comment: item.value,
            });
          }

          return results;
        },
        []
      );
    }

    // Notify the FE of our new results
    this.onResultsReady(autoCompleteResult);
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsIAutoCompleteSearch",
    "nsIAutoCompleteObserver",
  ]);
}

/**
 * SearchSuggestAutoComplete is a service implementation that handles suggest
 * results specific to web searches.
 *
 * @class
 */
export class SearchSuggestAutoComplete extends SuggestAutoComplete {
  classID = Components.ID("{aa892eb4-ffbf-477d-9f9a-06c995ae9f27}");
  serviceURL = "";
}
