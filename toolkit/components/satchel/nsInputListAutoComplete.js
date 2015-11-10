/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/nsFormAutoCompleteResult.jsm");

function InputListAutoComplete() {}

InputListAutoComplete.prototype = {
  classID       : Components.ID("{bf1e01d0-953e-11df-981c-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIInputListAutoComplete]),

  autoCompleteSearch : function (aUntrimmedSearchString, aField) {
    let [values, labels] = this.getListSuggestions(aField);
    let searchResult = values.length > 0 ? Ci.nsIAutoCompleteResult.RESULT_SUCCESS
                                         : Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
    let defaultIndex = values.length > 0 ? 0 : -1;
    return new FormAutoCompleteResult(aUntrimmedSearchString,
                                      searchResult, defaultIndex, "",
                                      values, labels, [], null);
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

var component = [InputListAutoComplete];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
