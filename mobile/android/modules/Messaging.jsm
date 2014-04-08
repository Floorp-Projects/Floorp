/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict"

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["sendMessageToJava"];

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

function sendMessageToJava(aMessage, aCallback) {
  if (aCallback) {
    let id = uuidgen.generateUUID().toString();
    let obs = {
      observe: function(aSubject, aTopic, aData) {
        let data = JSON.parse(aData);
        if (data.__guid__ != id) {
          return;
        }

        Services.obs.removeObserver(obs, aMessage.type + ":Response", false);

        if (data.status === "cancel") {
          // No Java-side listeners handled our callback.
          return;
        }

        aCallback(data.status === "success" ? data.response : null,
                  data.status === "error"   ? data.response : null);
      }
    }

    aMessage.__guid__ = id;
    Services.obs.addObserver(obs, aMessage.type + ":Response", false);
  }

  return Services.androidBridge.handleGeckoMessage(aMessage);
}
