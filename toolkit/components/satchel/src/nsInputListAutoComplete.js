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

const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/nsFormAutoCompleteResult.jsm");

function InputListAutoComplete() {}

InputListAutoComplete.prototype = {
  classID       : Components.ID("{bf1e01d0-953e-11df-981c-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIInputListAutoComplete]),

  autoCompleteSearch : function (formHistoryResult, aUntrimmedSearchString, aField) {
    let comments = []; // "comments" column values for suggestions
    let [values, labels] = this.getListSuggestions(aField);
    let historyResults = [];
    let historyComments = [];

    // formHistoryResult will be null if form autocomplete is disabled.
    // We still want the list values to display.
    if (formHistoryResult) {
      entries = formHistoryResult.wrappedJSObject.entries;
      for (let i = 0; i < entries.length; ++i) {
        historyResults.push(entries[i].text);
        historyComments.push("");
      }
    }

    // fill out the comment column for the suggestions
    // if we have any suggestions, put a label at the top
    if (values.length) {
      comments[0] = "separator";
    }
    for (let i = 1; i < values.length; ++i) {
      comments.push("");
    }

    // now put the history results above the suggestions
    let finalValues = historyResults.concat(values);
    let finalLabels = historyResults.concat(labels);
    let finalComments = historyComments.concat(comments);

    return new FormAutoCompleteResult(aUntrimmedSearchString,
                                      Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
                                      0, "", finalValues, finalLabels,
                                      finalComments, formHistoryResult);
  },

  getListSuggestions : function (aField) {
    let values = [];
    let labels = [];

    if (aField) {
      let filter = !aField.hasAttribute("mozNoFilter");
      let lowerFieldValue = aField.value.toLowerCase();

      if (aField.list) {
        let options = aField.list.options;
        let length = options.length;
        for (let i = 0; i < length; i++) {
          let item = options.item(i);
          let label = "";
          if (item.label) {
            label = item.label;
          } else if (item.text) {
            label = item.text;
          } else {
            label = item.value;
          }

          if (filter && label.toLowerCase().indexOf(lowerFieldValue) == -1) {
            continue;
          }

          labels.push(label);
          values.push(item.value);
        }
      }
    }

    return [values, labels];
  }
};

let component = [InputListAutoComplete];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
