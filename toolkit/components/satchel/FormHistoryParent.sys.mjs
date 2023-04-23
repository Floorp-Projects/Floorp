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
      case "FormHistory:FormSubmitEntries":
        this.#onFormSubmitEntries(message.data);
        break;

      case "FormHistory:AutoCompleteSearchAsync":
        return this.#onAutoCompleteSearch(message.data);

      case "FormHistory:RemoveEntry":
        this.#onRemoveEntry(message.data);
        break;
    }

    return undefined;
  }

  #onFormSubmitEntries(entries) {
    const changes = entries.map(entry => ({
      op: "bump",
      fieldname: entry.name,
      value: entry.value,
    }));

    lazy.FormHistory.update(changes);
  }

  #onAutoCompleteSearch({ searchString, params }) {
    return lazy.FormHistory.getAutoCompleteResults(searchString, params);
  }

  #onRemoveEntry({ inputName, value, guid }) {
    lazy.FormHistory.update({
      op: "remove",
      fieldname: inputName,
      value,
      guid,
    });
  }
}
