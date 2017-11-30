/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

function FormHistoryStartup() { }

FormHistoryStartup.prototype = {
  classID: Components.ID("{3A0012EB-007F-4BB8-AA81-A07385F77A25}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
    Ci.nsIFrameMessageListener,
  ]),

  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        FormHistory.updatePrefs();
        break;
      case "idle-daily":
      case "formhistory-expire-now":
        FormHistory.expireOldEntries();
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

    Services.ppmm.loadProcessScript("chrome://satchel/content/formSubmitListener.js", true);
    Services.ppmm.addMessageListener("FormHistory:FormSubmitEntries", this);

    let messageManager = Cc["@mozilla.org/globalmessagemanager;1"]
                         .getService(Ci.nsIMessageListenerManager);
    // For each of these messages, we could receive them from content,
    // or we might receive them from the ppmm if the searchbar is
    // having its history queried.
    for (let manager of [messageManager, Services.ppmm]) {
      manager.addMessageListener("FormHistory:AutoCompleteSearchAsync", this);
      manager.addMessageListener("FormHistory:RemoveEntry", this);
    }
  },

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

      case "FormHistory:AutoCompleteSearchAsync": {
        let { id, searchString, params } = message.data;

        if (this.pendingQuery) {
          this.pendingQuery.cancel();
          this.pendingQuery = null;
        }

        let mm;
        let query = null;
        if (message.target instanceof Ci.nsIMessageListenerManager) {
          // The target is the PPMM, meaning that the parent process
          // is requesting FormHistory data on the searchbar.
          mm = message.target;
        } else {
          // Otherwise, the target is a <xul:browser>.
          mm = message.target.messageManager;
        }

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
                mm.sendAsyncMessage("FormHistory:AutoCompleteSearchResults",
                                    { id, results });
              }
            }
          },
        };

        query = FormHistory.getAutoCompleteResults(searchString, params, processResults);
        this.pendingQuery = query;
        break;
      }

      case "FormHistory:RemoveEntry": {
        let { inputName, value, guid } = message.data;
        FormHistory.update({
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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FormHistoryStartup]);
