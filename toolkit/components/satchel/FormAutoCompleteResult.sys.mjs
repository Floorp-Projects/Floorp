/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class FormAutoCompleteResult {
  constructor(searchString, items, prevResult) {
    this.searchString = searchString;
    this._items = items;
    this._formHistResult = prevResult;
    this.entries = prevResult ? prevResult.wrappedJSObject.entries : [];
  }

  // nsISupports
  QueryInterface = ChromeUtils.generateQI(["nsIAutoCompleteResult"]);

  // The user's query string
  searchString = "";

  errorDescription = "";

  /**
   * A reference to the form history nsIAutocompleteResult that we're wrapping.
   * We use this to forward removeEntryAt calls as needed.
   */
  _formHistResult = null;

  entries = null;

  get wrappedJSObject() {
    return this;
  }

  /**
   * @returns {number} the result code of this result object, either:
   *         RESULT_IGNORED   (invalid searchString)
   *         RESULT_FAILURE   (failure)
   *         RESULT_NOMATCH   (no matches found)
   *         RESULT_SUCCESS   (matches found)
   */
  get searchResult() {
    return this._items.length
      ? Ci.nsIAutoCompleteResult.RESULT_SUCCESS
      : Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
  }

  /**
   * @returns {number} the default item that should be entered if none is selected
   */
  get defaultIndex() {
    return this._items.length ? 0 : -1;
  }

  /**
   * @returns {number} the number of results
   */
  get matchCount() {
    return this._items.length;
  }

  getAt(index) {
    if (index >= 0 && index < this._items.length) {
      return this._items[index];
    }

    throw Components.Exception(
      "Index out of range.",
      Cr.NS_ERROR_ILLEGAL_VALUE
    );
  }

  /**
   * Retrieves a result
   *
   * @param   {number} index   the index of the result requested
   * @returns {string}         the result at the specified index
   */
  getValueAt(index) {
    return this.getAt(index).value;
  }

  getLabelAt(index) {
    const item = this.getAt(index);
    return item.label || item.value;
  }

  /**
   * Retrieves a comment (metadata instance)
   *
   * @param {number} index    the index of the comment requested
   * @returns {object}        the comment at the specified index
   */
  getCommentAt(index) {
    return this.getAt(index).comment;
  }

  /**
   * Retrieves a style hint specific to a particular index.
   *
   * @param   {number} index   the index of the style hint requested
   * @returns {string|null}    the style hint at the specified index
   */
  getStyleAt(index) {
    this.getAt(index);

    if (index < this._formHistResult?.matchCount) {
      return "fromhistory";
    }

    if (
      this._formHistResult?.matchCount > 0 &&
      index == this._formHistResult.matchCount
    ) {
      return "datalist-first";
    }

    return null;
  }

  /**
   * Retrieves an image url.
   *
   * @param   {number} index  the index of the image url requested
   * @returns {string}        the image url at the specified index
   */
  getImageAt(index) {
    return "";
  }

  /**
   * Retrieves a result
   *
   * @param   {number} index   the index of the result requested
   * @returns {string}         the result at the specified index
   */
  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  }

  /**
   * Returns true if the value at the given index is removable
   *
   * @param   {number}  index  the index of the result to remove
   * @returns {boolean}        True if the value is removable
   */
  isRemovableAt(index) {
    return this.getAt(index).removable;
  }

  /**
   * Removes a result from the resultset
   *
   * @param {number}  index    the index of the result to remove
   */
  removeValueAt(index) {
    this.getAt(index);
    // Forward the removeValueAt call to the underlying result if we have one
    // Note: this assumes that the form history results were added to the top
    // of our arrays.
    if (this._formHistResult && index < this._formHistResult.matchCount) {
      // Delete the history result from the DB
      this._formHistResult.removeValueAt(index);
    }
    this._items.splice(index, 1);
  }
}
