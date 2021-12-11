/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/**
 * See nsIAutoCompleteSimpleSearch
 */
class AutoCompleteSimpleSearch {
  constructor() {
    this.classID = Components.ID("{dc185a77-ba88-4caa-8f16-465253f7599a}");
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIAutoCompleteSimpleSearch",
    ]);

    let initialState = Cc[
      "@mozilla.org/autocomplete/simple-result;1"
    ].createInstance(Ci.nsIAutoCompleteSimpleResult);
    initialState.setDefaultIndex(0);
    initialState.setSearchResult(Ci.nsIAutoCompleteResult.RESULT_NOMATCH);
    this.overrideNextResult(initialState);
  }

  _result = null;

  /**
   * See nsIAutoCompleteSimpleSearch
   */
  overrideNextResult(result) {
    this._result = result;
  }

  /**
   * See nsIAutoCompleteSearch
   */
  startSearch(searchString, searchParam, previousResult, listener) {
    let result = this._result;
    result.setSearchString(searchString);
    listener.onSearchResult(this, result);
  }

  /**
   * See nsIAutoCompleteSearch
   */
  stopSearch() {}
}

const EXPORTED_SYMBOLS = ["AutoCompleteSimpleSearch"];
