/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Zbarsky <dzbarsky@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let EXPORTED_SYMBOLS = [ "FormAutoCompleteResult" ];

const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function FormAutoCompleteResult(searchString,
                                searchResult,
                                defaultIndex,
                                errorDescription,
                                values,
                                labels,
                                comments,
                                prevResult) {
  this._searchString = searchString;
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
  _searchString: "",

  // The result code of this result object, see |get searchResult| for possible values.
  _searchResult: 0,

  // The default item that should be entered if none is selected
  _defaultIndex: 0,

  //The reason the search failed
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
   * @return the user's query string
   */
  get searchString() {
    return this._searchString;
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

  _checkIndexBounds : function (index) {
    if (index < 0 || index >= this._values.length) {
      throw Components.Exception("Index out of range.", Cr.NS_ERROR_ILLEGAL_VALUE);
    }
  },

  /**
   * Retrieves a result
   * @param  index    the index of the result requested
   * @return          the result at the specified index
   */
  getValueAt: function(index) {
    this._checkIndexBounds(index);
    return this._values[index];
  },

  getLabelAt: function(index) {
    this._checkIndexBounds(index);
    return this._labels[index];
  },

  /**
   * Retrieves a comment (metadata instance)
   * @param  index    the index of the comment requested
   * @return          the comment at the specified index
   */
  getCommentAt: function(index) {
    this._checkIndexBounds(index);
    return this._comments[index];
  },

  /**
   * Retrieves a style hint specific to a particular index.
   * @param  index    the index of the style hint requested
   * @return          the style hint at the specified index
   */
  getStyleAt: function(index) {
    this._checkIndexBounds(index);
    if (!this._comments[index]) {
      return null;  // not a category label, so no special styling
    }

    if (index == 0) {
      return "suggestfirst";  // category label on first line of results
    }

    return "suggesthint";   // category label on any other line of results
  },

  /**
   * Retrieves an image url.
   * @param  index    the index of the image url requested
   * @return          the image url at the specified index
   */
  getImageAt: function(index) {
    this._checkIndexBounds(index);
    return "";
  },

  /**
   * Removes a result from the resultset
   * @param  index    the index of the result to remove
   */
  removeValueAt: function(index, removeFromDatabase) {
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
