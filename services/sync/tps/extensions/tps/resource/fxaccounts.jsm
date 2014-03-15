/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "FxAccountsHelper",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://tps/logger.jsm");


/**
 * Helper object for Firefox Accounts authentication
 */
var FxAccountsHelper = {

  /**
   * Wrapper to synchronize the login of a user
   *
   * @param email
   *        The email address for the account (utf8)
   * @param password
   *        The user's password
   */
  signIn: function signIn(email, password) {
    let cb = Async.makeSpinningCallback();

    var client = new FxAccountsClient();
    client.signIn(email, password).then(credentials => {
      // Add keys because without those setSignedInUser() will fail
      credentials.kA = 'foo';
      credentials.kB = 'bar';

      Weave.Service.identity._fxaService.setSignedInUser(credentials).then(() => {
        cb(null);
      }, err => {
        cb(err);
      });
    }, (err) => {
      cb(err);
    });

    try {
      cb.wait();
    } catch (err) {
      Logger.logError("signIn() failed with: " + JSON.stringify(err));
      throw err;
    }
  }
};
