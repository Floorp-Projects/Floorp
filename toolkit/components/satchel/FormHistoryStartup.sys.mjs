/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
});

export class FormHistoryStartup {
  classID = Components.ID("{3A0012EB-007F-4BB8-AA81-A07385F77A25}");

  QueryInterface = ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]);

  observe(_subject, topic, _data) {
    switch (topic) {
      case "idle-daily":
      case "formhistory-expire-now":
        lazy.FormHistory.expireOldEntries().catch(console.error);
        break;
      case "profile-after-change":
        this.init();
        break;
    }
  }

  init() {
    if (this.inited) {
      return;
    }
    this.inited = true;

    // triggers needed service cleanup and db shutdown
    Services.obs.addObserver(this, "idle-daily", true);
    Services.obs.addObserver(this, "formhistory-expire-now", true);

    Services.ppmm.addMessageListener(
      "FormHistory:AutoCompleteSearchAsync",
      this
    );
    Services.ppmm.addMessageListener("FormHistory:RemoveEntry", this);
  }

  receiveMessage(message) {
    switch (message.name) {
      case "FormHistory:AutoCompleteSearchAsync":
        this.#onFormHistoryAutoCompleteSearchAsync({
          ...message.data,
          target: message.target,
        });
        break;

      case "FormHistory:RemoveEntry":
        this.#onFormHistoryRemoveEntry(message.data);
        break;
    }
  }

  async #onFormHistoryAutoCompleteSearchAsync({
    id,
    searchString,
    params,
    target,
  }) {
    // This case is only used for the search field. There is a
    // similar algorithm in FormHistoryParent.jsm that uses
    // sendQuery for other form fields.

    const instance = (this._queryInstance = {});
    const formHistoryEntries = await lazy.FormHistory.getAutoCompleteResults(
      searchString,
      params,
      () => this._queryInstance != instance
    );

    if (this._queryInstance == instance) {
      target.sendAsyncMessage("FormHistory:AutoCompleteSearchResults", {
        id,
        results: {
          formHistoryEntries,
          externalEntries: [],
        },
      });
    }
  }

  #onFormHistoryRemoveEntry({ inputName, value, guid }) {
    lazy.FormHistory.update({
      op: "remove",
      fieldname: inputName,
      value,
      guid,
    });
  }
}
