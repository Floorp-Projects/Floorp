/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Accounts"];

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

/**
 * A promise-based API for querying the existence of Sync accounts,
 * and accessing the Sync setup wizard.
 *
 * Usage:
 *
 *   Cu.import("resource://gre/modules/Accounts.jsm");
 *   Accounts.anySyncAccountsExist().then(
 *     (exist) => {
 *       console.log("Accounts exist? " + exist);
 *       if (!exist) {
 *         Accounts.launchSetup();
 *       }
 *     },
 *     (err) => {
 *       console.log("We failed so hard.");
 *     }
 *   );
 */
let Accounts = Object.freeze({
  _accountsExist: function (kind) {
    let deferred = Promise.defer();

    sendMessageToJava({
      type: "Accounts:Exist",
      kind: kind,
    }, (data, error) => {
      if (error) {
        deferred.reject(error);
      } else {
        deferred.resolve(data.exists);
      }
    });

    return deferred.promise;
  },

  firefoxAccountsExist: function () {
    return this._accountsExist("fxa");
  },

  syncAccountsExist: function () {
    return this._accountsExist("sync11");
  },

  anySyncAccountsExist: function () {
    return this._accountsExist("any");
  },

  /**
   * Fire-and-forget: open the Firefox accounts activity, which
   * will be the Getting Started screen if FxA isn't yet set up.
   *
   * There is no return value from this method.
   */
  launchSetup: function () {
    sendMessageToJava({
      type: "Accounts:Create",
    });
  },
});

