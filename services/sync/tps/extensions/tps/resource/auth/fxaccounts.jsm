/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Authentication",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://tps/logger.jsm");


/**
 * Helper object for Firefox Accounts authentication
 */
var Authentication = {

  /**
   * Check if an user has been logged in
   */
  get isLoggedIn() {
    return !!this.getSignedInUser();
  },

  /**
   * Wrapper to retrieve the currently signed in user
   *
   * @returns Information about the currently signed in user
   */
  getSignedInUser: function getSignedInUser() {
    let cb = Async.makeSpinningCallback();

    fxAccounts.getSignedInUser().then(user => {
      cb(null, user);
    }, error => {
      cb(error);
    })

    try {
      return cb.wait();
    } catch (error) {
      Logger.logError("getSignedInUser() failed with: " + JSON.stringify(error));
      throw error;
    }
  },

  /**
   * Wrapper to synchronize the login of a user
   *
   * @param account
   *        Account information of the user to login
   * @param account.username
   *        The username for the account (utf8)
   * @param account.password
   *        The user's password
   */
  signIn: function signIn(account) {
    let cb = Async.makeSpinningCallback();

    Logger.AssertTrue(account["username"], "Username has been found");
    Logger.AssertTrue(account["password"], "Password has been found");

    Logger.logInfo("Login user: " + account["username"] + '\n');

    let client = new FxAccountsClient();
    client.signIn(account["username"], account["password"]).then(credentials => {
      return fxAccounts.setSignedInUser(credentials);
    }).then(() => {
      cb(null, true);
    }, error => {
      cb(error, false);
    });

    try {
      cb.wait();

      if (Weave.Status.login !== Weave.LOGIN_SUCCEEDED) {
        Logger.logInfo("Logging into Weave.");
        Weave.Service.login();
        Logger.AssertEqual(Weave.Status.login, Weave.LOGIN_SUCCEEDED,
                           "Weave logged in");
      }

      return true;
    } catch (error) {
      throw new Error("signIn() failed with: " + error.message);
    }
  }
};
