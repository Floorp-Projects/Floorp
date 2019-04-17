/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "Authentication",
];

const {Log} = ChromeUtils.import("resource://gre/modules/Log.jsm");
const {clearTimeout, setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const {fxAccounts} = ChromeUtils.import("resource://gre/modules/FxAccounts.jsm");
const {FxAccountsClient} = ChromeUtils.import("resource://gre/modules/FxAccountsClient.jsm");
const {FxAccountsConfig} = ChromeUtils.import("resource://gre/modules/FxAccountsConfig.jsm");
const {Logger} = ChromeUtils.import("resource://tps/logger.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

/**
 * Helper object for Firefox Accounts authentication
 */
var Authentication = {

  /**
   * Check if an user has been logged in
   */
  async isLoggedIn() {
    return !!(await this.getSignedInUser());
  },

  async isReady() {
    let user = await this.getSignedInUser();
    return user && user.verified;
  },

  _getRestmailUsername(user) {
    const restmailSuffix = "@restmail.net";
    if (user.toLowerCase().endsWith(restmailSuffix)) {
      return user.slice(0, -restmailSuffix.length);
    }
    return null;
  },

  async shortWaitForVerification(ms) {
    let userData = await this.getSignedInUser();
    let timeoutID;
    let timeoutPromise = new Promise(resolve => {
      timeoutID = setTimeout(() => {
        Logger.logInfo(`Warning: no verification after ${ms}ms.`);
        resolve();
      }, ms);
    });
    await Promise.race([
      fxAccounts.whenVerified(userData)
                .finally(() => clearTimeout(timeoutID)),
      timeoutPromise,
    ]);
    userData = await this.getSignedInUser();
    return userData && userData.verified;
  },

  async _openVerificationPage(uri) {
    let mainWindow = Services.wm.getMostRecentWindow("navigator:browser");
    let newtab = mainWindow.gBrowser.addWebTab(uri);
    let win = mainWindow.gBrowser.getBrowserForTab(newtab);
    await new Promise(resolve => {
      win.addEventListener("loadend", resolve, { once: true });
    });
    let didVerify = await this.shortWaitForVerification(10000);
    mainWindow.gBrowser.removeTab(newtab);
    return didVerify;
  },

  async _completeVerification(user) {
    let username = this._getRestmailUsername(user);
    if (!username) {
      Logger.logInfo(`Username "${user}" isn't a restmail username so can't complete verification`);
      return false;
    }
    Logger.logInfo("Fetching mail (from restmail) for user " + username);
    let restmailURI = `https://www.restmail.net/mail/${encodeURIComponent(username)}`;
    let triedAlready = new Set();
    const tries = 10;
    const normalWait = 2000;
    for (let i = 0; i < tries; ++i) {
      let resp = await fetch(restmailURI);
      let messages = await resp.json();
      // Sort so that the most recent emails are first.
      messages.sort((a, b) => new Date(b.receivedAt) - new Date(a.receivedAt));
      for (let m of messages) {
        // We look for a link that has a x-link that we haven't yet tried.
        if (!m.headers["x-link"] || triedAlready.has(m.headers["x-link"])) {
          continue;
        }
        let confirmLink = m.headers["x-link"];
        triedAlready.add(confirmLink);
        Logger.logInfo("Trying confirmation link " + confirmLink);
        try {
          if (await this._openVerificationPage(confirmLink)) {
            return true;
          }
        } catch (e) {
          Logger.logInfo("Warning: Failed to follow confirmation link: " + Log.exceptionStr(e));
        }
      }
      if (i === 0) {
        // first time through after failing we'll do this.
        await fxAccounts.resendVerificationEmail();
      }
      if (await this.shortWaitForVerification(normalWait)) {
        return true;
      }
    }
    // One last try.
    return this.shortWaitForVerification(normalWait);
  },

  async deleteEmail(user) {
    let username = this._getRestmailUsername(user);
    if (!username) {
      Logger.logInfo("Not a restmail username, can't delete");
      return false;
    }
    Logger.logInfo("Deleting mail (from restmail) for user " + username);
    let restmailURI = `https://www.restmail.net/mail/${encodeURIComponent(username)}`;
    try {
      // Clean up after ourselves.
      let deleteResult = await fetch(restmailURI, { method: "DELETE" });
      if (!deleteResult.ok) {
        Logger.logInfo(`Warning: Got non-success status ${deleteResult.status} when deleting emails`);
        return false;
      }
    } catch (e) {
      Logger.logInfo("Warning: Failed to delete old emails: " + Log.exceptionStr(e));
      return false;
    }
    return true;
  },

  /**
   * Wrapper to retrieve the currently signed in user
   *
   * @returns Information about the currently signed in user
   */
  async getSignedInUser() {
    try {
      return (await fxAccounts.getSignedInUser());
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
  async signIn(account) {
    Logger.AssertTrue(account.username, "Username has been found");
    Logger.AssertTrue(account.password, "Password has been found");

    Logger.logInfo("Login user: " + account.username);

    try {
      // Required here since we don't go through the real login page
      await FxAccountsConfig.ensureConfigured();

      let client = new FxAccountsClient();
      let credentials = await client.signIn(account.username, account.password, true);
      await fxAccounts.setSignedInUser(credentials);
      if (!credentials.verified) {
        await this._completeVerification(account.username);
      }

      return true;
    } catch (error) {
      throw new Error("signIn() failed with: " + error.message);
    }
  },

  /**
   * Sign out of Firefox Accounts.
   */
  async signOut() {
    if (await Authentication.isLoggedIn()) {
      // Note: This will clean up the device ID.
      await fxAccounts.signOut();
    }
  },
};
