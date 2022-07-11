/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "FormHistory",
  "resource://gre/modules/FormHistory.jsm"
);

function FormHistoryStartup() {}

FormHistoryStartup.prototype = {
  classID: Components.ID("{3A0012EB-007F-4BB8-AA81-A07385F77A25}"),

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        lazy.FormHistory.updatePrefs();
        break;
      case "idle-daily":
      case "formhistory-expire-now":
        lazy.FormHistory.expireOldEntries();
        break;
      case "profile-after-change":
        this.init();
        break;
    }
  },

  inited: false,
  pendingQuery: null,

  init() {
    if (this.inited) {
      return;
    }
    this.inited = true;

    Services.prefs.addObserver("browser.formfill.", this, true);

    // triggers needed service cleanup and db shutdown
    Services.obs.addObserver(this, "idle-daily", true);
    Services.obs.addObserver(this, "formhistory-expire-now", true);

    Services.ppmm.addMessageListener(
      "FormHistory:AutoCompleteSearchAsync",
      this
    );
    Services.ppmm.addMessageListener("FormHistory:RemoveEntry", this);
  },

  receiveMessage(message) {
    switch (message.name) {
      case "FormHistory:AutoCompleteSearchAsync": {
        // This case is only used for the search field. There is a
        // similar algorithm in FormHistoryParent.jsm that uses
        // sendQuery for other form fields.

        let { id, searchString, params } = message.data;

        if (this.pendingQuery) {
          this.pendingQuery.cancel();
          this.pendingQuery = null;
        }

        let query = null;

        let results = [];
        let processResults = {
          handleResult: aResult => {
            results.push(aResult);
          },
          handleCompletion: aReason => {
            // Check that the current query is still the one we created. Our
            // query might have been canceled shortly before completing, in
            // that case we don't want to call the callback anymore.
            if (query === this.pendingQuery) {
              this.pendingQuery = null;
              if (!aReason) {
                message.target.sendAsyncMessage(
                  "FormHistory:AutoCompleteSearchResults",
                  {
                    id,
                    results,
                  }
                );
              }
            }
          },
        };

        query = lazy.FormHistory.getAutoCompleteResults(
          searchString,
          params,
          processResults
        );
        this.pendingQuery = query;
        break;
      }

      case "FormHistory:RemoveEntry": {
        let { inputName, value, guid } = message.data;
        lazy.FormHistory.update({
          op: "remove",
          fieldname: inputName,
          value,
          guid,
        });
        break;
      }
    }
  },
};

var EXPORTED_SYMBOLS = ["FormHistoryStartup"];
