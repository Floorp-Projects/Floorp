/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FormAutoCompleteResult } from "resource://gre/modules/nsFormAutoCompleteResult.sys.mjs";

export class InputListAutoComplete {
  classID = Components.ID("{bf1e01d0-953e-11df-981c-0800200c9a66}");
  QueryInterface = ChromeUtils.generateQI(["nsIInputListAutoComplete"]);

  autoCompleteSearch(aUntrimmedSearchString, aField) {
    const items = this.getListSuggestions(aField);
    const searchResult = items.length
      ? Ci.nsIAutoCompleteResult.RESULT_SUCCESS
      : Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
    const defaultIndex = items.length ? 0 : -1;

    return new FormAutoCompleteResult(
      aUntrimmedSearchString,
      searchResult,
      defaultIndex,
      "",
      items,
      null
    );
  }

  getListSuggestions(aField) {
    const items = [];

    if (!aField?.list) {
      return items;
    }

    const lowerFieldValue = aField.value.toLowerCase();

    for (const option of aField.list.options) {
      const label = option.label || option.text || option.value || "";

      if (!label.toLowerCase().includes(lowerFieldValue)) {
        continue;
      }

      items.push({
        label,
        value: option.value,
        comment: "",
        removable: false,
      });
    }

    return items;
  }
}
