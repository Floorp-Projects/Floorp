/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "FormAutoCompleteResult" ];

const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

this.FormAutoCompleteResult =
 function FormAutoCompleteResult(searchString,
                                 searchResult,
                                 defaultIndex,
                                 errorDescription,
                                 values,
                                 labels,
                                 comments,
                                 prevResult) {
  this.searchString = searchString;
  this._searchResult = searchResult;
  this._defaultIndex = defaultIndex;
  this._errorDescription = errorDescription;
  this._values = values;
  this._labels = labels;
  this._comments = comments;
  this._formHistResult = prevResult;

  if (prevResult) {
    this.entries = prevResult.wrappedJSObject.entries;
  } else {
    this.entries = [];
  }
}

FormAutoCompleteResult.prototype = {

  // The user's query string
  searchString: "",

  // The result code of this result object, see |get searchResult| for possible values.
  _searchResult: 0,

  // The default item that should be entered if none is selected
  _defaultIndex: 0,

  // The reason the search failed
  _errorDescription: "",

  /**
   * A reference to the form history nsIAutocompleteResult that we're wrapping.
   * We use this to forward removeEntryAt calls as needed.
   */
  _formHistResult: null,

  entries: null,

  get wrappedJSObject() {
    return this;
  },

  /**
   * @return the result code of this result object, either:
   *         RESULT_IGNORED   (invalid searchString)
   *         RESULT_FAILURE   (failure)
   *         RESULT_NOMATCH   (no matches found)
   *         RESULT_SUCCESS   (matches found)
   */
  get searchResult() {
    return this._searchResult;
  },

  /**
   * @return the default item that should be entered if none is selected
   */
  get defaultIndex() {
    return this._defaultIndex;
  },

  /**
   * @return the reason the search failed
   */
  get errorDescription() {
    return this._errorDescription;
  },

  /**
   * @return the number of results
   */
  get matchCount() {
    return this._values.length;
  },

  _checkIndexBounds(index) {
    if (index < 0 || index >= this._values.length) {
      throw Components.Exception("Index out of range.", Cr.NS_ERROR_ILLEGAL_VALUE);
    }
  },

  /**
   * Retrieves a result
   * @param  index    the index of the result requested
   * @return          the result at the specified index
   */
  getValueAt(index) {
    this._checkIndexBounds(index);
    return this._values[index];
  },

  getLabelAt(index) {
    this._checkIndexBounds(index);
    return this._labels[index] || this._values[index];
  },

  /**
   * Retrieves a comment (metadata instance)
   * @param  index    the index of the comment requested
   * @return          the comment at the specified index
   */
  getCommentAt(index) {
    this._checkIndexBounds(index);
    return this._comments[index];
  },

  /**
   * Retrieves a style hint specific to a particular index.
   * @param  index    the index of the style hint requested
   * @return          the style hint at the specified index
   */
  getStyleAt(index) {
    this._checkIndexBounds(index);

    if (this._formHistResult && index < this._formHistResult.matchCount) {
      return "fromhistory";
    }

    if (this._formHistResult &&
        this._formHistResult.matchCount > 0 &&
        index == this._formHistResult.matchCount) {
      return "datalist-first";
    }

    return null;
  },

  /**
   * Retrieves an image url.
   * @param  index    the index of the image url requested
   * @return          the image url at the specified index
   */
  getImageAt(index) {
    this._checkIndexBounds(index);
    return "";
  },

  /**
   * Retrieves a result
   * @param  index    the index of the result requested
   * @return          the result at the specified index
   */
  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  },

  /**
   * Removes a result from the resultset
   * @param  index    the index of the result to remove
   */
  removeValueAt(index, removeFromDatabase) {
    this._checkIndexBounds(index);
    // Forward the removeValueAt call to the underlying result if we have one
    // Note: this assumes that the form history results were added to the top
    // of our arrays.
    if (removeFromDatabase && this._formHistResult &&
        index < this._formHistResult.matchCount) {
      // Delete the history result from the DB
      this._formHistResult.removeValueAt(index, true);
    }
    this._values.splice(index, 1);
    this._labels.splice(index, 1);
    this._comments.splice(index, 1);
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteResult])
};
