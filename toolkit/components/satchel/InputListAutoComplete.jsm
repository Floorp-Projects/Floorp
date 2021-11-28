/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FormAutoCompleteResult } = ChromeUtils.import(
  "resource://gre/modules/nsFormAutoCompleteResult.jsm"
);

function InputListAutoComplete() {}

InputListAutoComplete.prototype = {
  classID: Components.ID("{bf1e01d0-953e-11df-981c-0800200c9a66}"),
  QueryInterface: ChromeUtils.generateQI(["nsIInputListAutoComplete"]),

  autoCompleteSearch(aUntrimmedSearchString, aField) {
    let items = this.getListSuggestions(aField);
    let searchResult = items.length
      ? Ci.nsIAutoCompleteResult.RESULT_SUCCESS
      : Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
    let defaultIndex = items.length ? 0 : -1;

    return new FormAutoCompleteResult(
      aUntrimmedSearchString,
      searchResult,
      defaultIndex,
      "",
      items,
      null
    );
  },

  getListSuggestions(aField) {
    let items = [];
    if (!aField || !aField.list) {
      return items;
    }

    let filter = !aField.hasAttribute("mozNoFilter");
    let lowerFieldValue = aField.value.toLowerCase();
    let options = aField.list.options;

    for (let item of options) {
      let label = "";
      if (item.label) {
        label = item.label;
      } else if (item.text) {
        label = item.text;
      } else {
        label = item.value;
      }

      if (filter && !label.toLowerCase().includes(lowerFieldValue)) {
        continue;
      }

      items.push({
        label,
        value: item.value,
        comment: "",
        removable: false,
      });
    }

    return items;
  },
};

var EXPORTED_SYMBOLS = ["InputListAutoComplete"];
