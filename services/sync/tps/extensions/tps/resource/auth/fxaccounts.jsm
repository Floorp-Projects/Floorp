/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Authentication",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsConfig.jsm");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://tps/logger.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

Cu.importGlobalProperties(["fetch"]);

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

  _getRestmailUsername(user) {
    const restmailSuffix = "@restmail.net";
    if (user.toLowerCase().endsWith(restmailSuffix)) {
      return user.slice(0, -restmailSuffix.length);
    }
    return null;
  },

  async shortWaitForVerification(ms) {
    let userData = this.getSignedInUser();
    await Promise.race([
      fxAccounts.whenVerified(userData),
      new Promise(resolve => {
        setTimeout(() => {
          Logger.logInfo(`Warning: no verification after ${ms}ms.`);
          resolve();
        }, ms);
      })
    ]);
    userData = this.getSignedInUser();
    return userData && userData.verified;
  },

  async _openVerificationPage(uri) {
    let mainWindow = Services.wm.getMostRecentWindow("navigator:browser");
    let newtab = mainWindow.getBrowser().addTab(uri);
    let win = mainWindow.getBrowser().getBrowserForTab(newtab);
    await new Promise(resolve => {
      win.addEventListener("loadend", resolve, { once: true });
    });
    let didVerify = await this.shortWaitForVerification(10000);
    mainWindow.getBrowser().removeTab(newtab);
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
      if (await this.shortWaitForVerification(normalWait)) {
        return true;
      }
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
      await this._completeVerification(account.username);

      if (Weave.Status.login !== Weave.LOGIN_SUCCEEDED) {
        Logger.logInfo("Logging into Weave.");
        await Weave.Service.login();
        Logger.AssertEqual(Weave.Status.login, Weave.LOGIN_SUCCEEDED,
                           "Weave logged in");
      }
      return true;
    } catch (error) {
      throw new Error("signIn() failed with: " + error.message);
    }
  },

  /**
   * Sign out of Firefox Accounts. It also clears out the device ID, if we find one.
   */
  async signOut() {
    if (await Authentication.isLoggedIn()) {
      let user = await Authentication.getSignedInUser();
      if (!user) {
        throw new Error("Failed to get signed in user!");
      }
      let fxc = new FxAccountsClient();
      let { sessionToken, deviceId } = user;
      if (deviceId) {
        Logger.logInfo("Destroying device " + deviceId);
        await fxAccounts.deleteDeviceRegistration(sessionToken, deviceId);
        await fxAccounts.signOut(true);
      } else {
        Logger.logError("No device found.");
        await fxc.signOut(sessionToken, { service: "sync" });
      }
    }
  }
};
