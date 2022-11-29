/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
});

export class FormHistoryParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "FormHistory:FormSubmitEntries": {
        let entries = message.data;
        let changes = entries.map(entry => ({
          op: "bump",
          fieldname: entry.name,
          value: entry.value,
        }));

        lazy.FormHistory.update(changes);
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
    return lazy.FormHistory.getAutoCompleteResults(searchString, params);
  }

  removeEntry(message) {
    let { inputName, value, guid } = message.data;
    lazy.FormHistory.update({
      op: "remove",
      fieldname: inputName,
      value,
      guid,
    });
  }
}
