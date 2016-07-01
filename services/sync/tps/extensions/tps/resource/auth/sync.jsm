/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Authentication",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-sync/main.js");
Cu.import("resource://tps/logger.jsm");


/**
 * Helper object for deprecated Firefox Sync authentication
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
    let user = null;

    if (Weave.Service.isLoggedIn) {
      user = {
        email: Weave.Service.identity.account,
        password: Weave.Service.identity.basicPassword,
        passphrase: Weave.Service.identity.syncKey
      };
    }

    return user;
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
   * @param account.passphrase
   *        The users's passphrase
   */
  signIn: function signIn(account) {
    Logger.AssertTrue(account["username"], "Username has been found");
    Logger.AssertTrue(account["password"], "Password has been found");
    Logger.AssertTrue(account["passphrase"], "Passphrase has been found");

    Logger.logInfo("Logging in user: " + account["username"]);

    Weave.Service.identity.account = account["username"];
    Weave.Service.identity.basicPassword = account["password"];
    Weave.Service.identity.syncKey = account["passphrase"];

    if (Weave.Status.login !== Weave.LOGIN_SUCCEEDED) {
      Logger.logInfo("Logging into Weave.");
      Weave.Service.login();
      Logger.AssertEqual(Weave.Status.login, Weave.LOGIN_SUCCEEDED,
                         "Weave logged in");

      // Bug 997279: Temporary workaround until we can ensure that Sync itself
      // sends this notification for the first login attempt by TPS
      Weave.Svc.Obs.notify("weave:service:setup-complete");
    }

    return true;
  },

  signOut() {
    Weave.Service.logout();
  }
};
