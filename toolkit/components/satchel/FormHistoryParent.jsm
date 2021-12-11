/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["FormHistoryParent"];

ChromeUtils.defineModuleGetter(
  this,
  "FormHistory",
  "resource://gre/modules/FormHistory.jsm"
);

class FormHistoryParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "FormHistory:FormSubmitEntries": {
        let entries = message.data;
        let changes = entries.map(entry => ({
          op: "bump",
          fieldname: entry.name,
          value: entry.value,
        }));

        FormHistory.update(changes);
        break;
      }

      case "FormHistory:AutoCompleteSearchAsync":
        return this.autoCompleteSearch(message);

      case "FormHistory:RemoveEntry":
        this.removeEntry(message);
        break;
    }

    return undefined;
  }

  autoCompleteSearch(message) {
    let { searchString, params } = message.data;

    return new Promise((resolve, reject) => {
      let results = [];
      let processResults = {
        handleResult: aResult => {
          results.push(aResult);
        },
        handleCompletion: aReason => {
          if (!aReason) {
            resolve(results);
          } else {
            reject();
          }
        },
      };

      FormHistory.getAutoCompleteResults(searchString, params, processResults);
    });
  }

  removeEntry(message) {
    let { inputName, value, guid } = message.data;
    FormHistory.update({
      op: "remove",
      fieldname: inputName,
      value,
      guid,
    });
  }
}
